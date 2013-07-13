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
 * Purpose: Implementation of an abstract media source
 * Author:  Thomas Volkert
 * Since:   2008-12-02
 */

#include <Header_Ffmpeg.h>
#include <MediaSource.h>
#include <Logger.h>
#include <HBSystem.h>

#include <string>
#include <string.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Base;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

// how many timestamp should be used to emulate a desired input frame rate?
#define MEDIA_SOURCE_RT_GRABBING_TIMESTAMP_HISTORY                             128

// define the threshold for silence detection
#define MEDIA_SOURCE_DEFAULT_SILENCE_THRESHOLD					               128

//de/activate VDPAU support
//#define MEDIA_SOURCE_VDPAU_SUPPORT

// de/activate MT support during video encoding: ffmpeg supports MT only for encoding
#define MEDIA_SOURCE_RECORDER_MULTI_THREADED_VIDEO_ENCODING

///////////////////////////////////////////////////////////////////////////////

Mutex MediaSource::mFfmpegInitMutex;
bool MediaSource::mFfmpegInitiated = false;

MediaSource::MediaSource(string pName):
    PacketStatistic(pName)
{
    mSourceTimeShiftForRTGrabbing = 0;
    mRelativeLoss = 0;
    mDecoderSynchPoints = 0;
    mEndToEndDelay = 0;
    mDecodedIFrames = 0;
    mDecodedPFrames = 0;
    mDecodedBFrames = 0;
    mInputStartPts = 0;
	mDecodedSFrames = 0;
	mDecodedSIFrames = 0;
	mDecodedSPFrames = 0;
	mDecodedBIFrames = 0;
	mDecoderOutputFrameDelay = 0;
	mAudioSilenceThreshold = MEDIA_SOURCE_DEFAULT_SILENCE_THRESHOLD;
	mDecoderFrameBufferTime = 0;
    mDecoderFrameBufferTimeMax = 0;
    mDecoderFramePreBufferTime = 0;
    mSourceType = SOURCE_ABSTRACT;
    mMarkerActivated = false;
    mMediaSourceOpened = false;
    mDecoderFramePreBufferingAutoRestart = false;
    mGrabbingStopped = false;
    mRecording = false;
    SetRtpActivation(true);
    mCodecContext = NULL;
    mResampleBuffer = NULL;
    mRecorderFinalFrame = NULL;
    mRecorderResampleBuffer = NULL;
    mRecorderCodecContext = NULL;
    mRecorderFormatContext = NULL;
    mRecorderAudioResampleContext = NULL;
    mRecorderVideoScalerContext = NULL;
    mAudioResampleContext = NULL;
    mInputAudioFormat = AV_SAMPLE_FMT_S16;
    mOutputAudioFormat = AV_SAMPLE_FMT_S16;
    mVideoScalerContext = NULL;
    mFormatContext = NULL;
    mRecordingSaveFileName = "";
    mDesiredDevice = "";
    mCurrentDevice = "";
    mCurrentDeviceName = "";
    mLastGrabResultWasError = false;
    mNumberOfFrames = 0;
    mFrameNumber = 0;
    mChunkDropCounter = 0;
    mInputAudioChannels = -1;
    mInputAudioSampleRate = -1;
    mInputBitRate = -1;
    mOutputAudioSampleRate = -1;
    mOutputAudioChannels = -1;
    mSourceResX = 352;
    mSourceResY = 288;
    mTargetResX = 352;
    mTargetResY = 288;
    mInputFrameRate = 29.97;
    mOutputFrameRate = 29.97;
    mCurrentInputChannel = 0;
    mDesiredInputChannel = 0;
    mRTGrabbingFrameTimestamps.clear();
    mMediaType = MEDIA_UNKNOWN;
    for (int i = 0; i < MEDIA_SOURCE_MAX_AUDIO_CHANNELS; i++)
		mResampleFifo[i] = NULL;

    FfmpegInit();

    // ###################################################################
    // ### add all 6 default video formats to the list of supported ones
    // ###################################################################
    VideoFormatDescriptor tFormat;

    tFormat.Name="SQCIF";      //      128 �  96
    tFormat.ResX = 128;
    tFormat.ResY = 96;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="QCIF";       //      176 � 144
    tFormat.ResX = 176;
    tFormat.ResY = 144;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="CIF";        //      352 � 288
    tFormat.ResX = 352;
    tFormat.ResY = 288;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="CIF4";       //      704 � 576
    tFormat.ResX = 704;
    tFormat.ResY = 576;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="DVD";        //      720 � 576
    tFormat.ResX = 720;
    tFormat.ResY = 576;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="CIF9";       //     1056 � 864
    tFormat.ResX = 1056;
    tFormat.ResY = 864;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="EDTV";       //     1280 � 720
    tFormat.ResX = 1280;
    tFormat.ResY = 720;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="CIF16";      //     1408 � 1152
    tFormat.ResX = 1408;
    tFormat.ResY = 1152;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="HDTV";       //     1920 � 1080
    tFormat.ResX = 1920;
    tFormat.ResY = 1080;
    mSupportedVideoFormats.push_back(tFormat);
}

MediaSource::~MediaSource()
{
    DeleteAllRegisteredMediaSinks();
}

///////////////////////////////////////////////////////////////////////////////

void MediaSource::FfmpegInit()
{
    mFfmpegInitMutex.lock();
    if(!mFfmpegInitiated)
    {
        LOGEX(MediaSource, LOG_VERBOSE, "Initializing ffmpeg libraries..");
        LOGEX(MediaSource, LOG_VERBOSE, "..current log level: %d", LOGGER.GetLogLevel());

        // console logging of FFMPG
        if (LOGGER.GetLogLevel() == LOG_WORLD)
            av_log_set_level(AV_LOG_DEBUG);
        else if (LOGGER.GetLogLevel() == LOG_VERBOSE)
            av_log_set_level(AV_LOG_VERBOSE);
        else
            av_log_set_level(AV_LOG_QUIET);

        avcodec_register_all();

        // register all supported input and output devices
        avdevice_register_all();

        // register all supported media filters
        //avfilter_register_all();

        // register all formats and codecs
        av_register_all();

        #if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53, 32, 100)
            // init network support once instead for every stream
            avformat_network_init();
        #endif

        // register our own lock manager at ffmpeg
        //HINT: we can do this as many times as this class is instanced
        if(av_lockmgr_register(FfmpegLockManager))
            LOGEX(MediaSource, LOG_ERROR, "Registration of own lock manager at ffmpeg failed.");

        mFfmpegInitiated = true;
    }
    mFfmpegInitMutex.unlock();
}

void MediaSource::LogSupportedVideoCodecs(bool pSendToLoggerOnly)
{
    FfmpegInit();

    AVCodec *tCodec = av_codec_next(NULL);
    AVCodec *tNextCodec = av_codec_next(tCodec);

    string tIntro = "Supported video codecs:\n"
           " D . = Decoding supported\n"
           " . E = Encoding supported\n";

    if (pSendToLoggerOnly)
        LOGEX(MediaSource, LOG_VERBOSE, "%s", tIntro.c_str());
    else
        printf("%s\n");

    while ((tCodec != NULL))
    {
        if (tCodec->type == AVMEDIA_TYPE_VIDEO)
        {
            bool tDecode = (tCodec->decode != NULL);
            bool tEncode = false;
            #ifndef FF_API_OLD_ENCODE_AUDIO
                tEncode = (tCodec->encode != NULL);
            #else
                tEncode = (tCodec->encode2 != NULL);
            #endif

            if ((tNextCodec != NULL) && (strcmp(tCodec->name, tNextCodec->name) == 0))
            {
	            #ifndef FF_API_OLD_ENCODE_AUDIO
                    tEncode |= (tNextCodec->encode != NULL);
                #else
                    tEncode |= (tNextCodec->encode2 != NULL);
                #endif
                tDecode |= (tNextCodec->decode != NULL);
                tCodec = tNextCodec;
            }
            if (pSendToLoggerOnly)
                LOGEX(MediaSource, LOG_VERBOSE, " %s %s  %-15s %s",
                   tDecode ? "D" : " ",
                   tEncode ? "E" : " ",
                   tCodec->name,
                   tCodec->long_name ? tCodec->long_name : "");
            else
                printf(" %s %s  %-15s %s\n",
                   tDecode ? "D" : " ",
                   tEncode ? "E" : " ",
                   tCodec->name,
                   tCodec->long_name ? tCodec->long_name : "");
        }

        // go to next
        tCodec = av_codec_next(tCodec);
        if (tCodec != NULL)
            tNextCodec = av_codec_next(tCodec);
    }
    Thread::Suspend(1 * 1000 * 1000);
}

void MediaSource::LogSupportedAudioCodecs(bool pSendToLoggerOnly)
{
    FfmpegInit();

    AVCodec *tCodec = av_codec_next(NULL);
    AVCodec *tNextCodec = av_codec_next(tCodec);

    string tIntro = "Supported video codecs:\n"
           " D . = Decoding supported\n"
           " . E = Encoding supported\n";

    if (pSendToLoggerOnly)
        LOGEX(MediaSource, LOG_VERBOSE, "%s", tIntro.c_str());
    else
        printf("%s\n");

    while ((tCodec != NULL))
    {
        if (tCodec->type == AVMEDIA_TYPE_AUDIO)
        {
//            printf("Found audio codec: %s %s  %-15s %s\n",
//               tCodec->decode ? "D" : " ",
//               tCodec->encode ? "E" : " ",
//               tCodec->name,
//               tCodec->long_name ? tCodec->long_name : "");
//            printf("Found audio next codec: %s %s  %-15s %s\n",
//				tNextCodec->decode ? "D" : " ",
//				tNextCodec->encode ? "E" : " ",
//				tNextCodec->name,
//				tNextCodec->long_name ? tCodec->long_name : "");
            bool tDecode = (tCodec->decode != NULL);
            bool tEncode = false;
            #ifndef FF_API_OLD_ENCODE_AUDIO
                tEncode = (tCodec->encode != NULL);
            #else
                tEncode = (tCodec->encode2 != NULL);
            #endif
            if ((tNextCodec != NULL) && (strcmp(tCodec->name, tNextCodec->name) == 0))
            {
                #ifndef FF_API_OLD_ENCODE_AUDIO
                    tEncode |= (tNextCodec->encode != NULL);
                #else
                    tEncode |= (tNextCodec->encode2 != NULL);
                #endif
                tDecode |= (tNextCodec->decode != NULL);
                tCodec = tNextCodec;
            }
            if (pSendToLoggerOnly)
                LOGEX(MediaSource, LOG_VERBOSE, " %s %s  %-15s %s",
                   tDecode ? "D" : " ",
                   tEncode ? "E" : " ",
                   tCodec->name,
                   tCodec->long_name ? tCodec->long_name : "");
            else
                printf(" %s %s  %-15s %s\n",
                   tDecode ? "D" : " ",
                   tEncode ? "E" : " ",
                   tCodec->name,
                   tCodec->long_name ? tCodec->long_name : "");
        }

        // go to next
        tCodec = av_codec_next(tCodec);
        if (tCodec != NULL)
            tNextCodec = av_codec_next(tCodec);
    }
    Thread::Suspend(1 * 1000 * 1000);
}

void MediaSource::LogSupportedInputFormats(bool pSendToLoggerOnly)
{
    FfmpegInit();

    AVInputFormat *tFormat = av_iformat_next(NULL);

    string tIntro = "Supported input formats:\n";

    if (pSendToLoggerOnly)
        LOGEX(MediaSource, LOG_VERBOSE, "%s", tIntro.c_str());
    else
        printf("%s\n");

    while ((tFormat != NULL))
    {
        if (pSendToLoggerOnly)
            LOGEX(MediaSource, LOG_VERBOSE, " %-15s %s",
                tFormat->name,
                tFormat->long_name ? tFormat->long_name : "");
        else
            printf(" %-15s %s\n",
                tFormat->name,
                tFormat->long_name ? tFormat->long_name : "");

        // go to next
        tFormat = av_iformat_next(tFormat);
    }
    Thread::Suspend(1 * 1000 * 1000);
}

void MediaSource::LogSupportedOutputFormats(bool pSendToLoggerOnly)
{
    FfmpegInit();

    AVOutputFormat *tFormat = av_oformat_next(NULL);

    string tIntro = "Supported output formats:\n";

    if (pSendToLoggerOnly)
        LOGEX(MediaSource, LOG_VERBOSE, "%s", tIntro.c_str());
    else
        printf("%s\n");

    while ((tFormat != NULL))
    {
        if (pSendToLoggerOnly)
            LOGEX(MediaSource, LOG_VERBOSE, " %-15s %s",
                tFormat->name,
                tFormat->long_name ? tFormat->long_name : "");
        else
            printf(" %-15s %s\n",
                tFormat->name,
                tFormat->long_name ? tFormat->long_name : "");

        // go to next
        tFormat = av_oformat_next(tFormat);
    }
    Thread::Suspend(1 * 1000 * 1000);
}

///////////////////////////////////////////////////////////////////////////////

bool MediaSource::SupportsMuxing()
{
    return false;
}

string MediaSource::GetMuxingCodec()
{
    return "";
}

void MediaSource::GetMuxingResolution(int &pResX, int &pResY)
{
    pResX = 0;
    pResY = 0;
}

int MediaSource::GetMuxingBufferCounter()
{
    return 0;
}

int MediaSource::GetMuxingBufferSize()
{
    return 0;
}

MediaSource* MediaSource::GetMediaSource()
{
    LOG(LOG_VERBOSE, "This is only the dummy GetMediaSource() function");
    return this;
}

int MediaSource::FfmpegLockManager(void **pMutex, enum AVLockOp pMutexOperation)
{
    int tTmpResult = 1;
    Mutex *tMutex;

    switch(pMutexOperation)
    {
        case AV_LOCK_CREATE:
            #ifdef MS_DEBUG_FFMPEG_MUTEX
                LOG(LOG_VERBOSE, "Creating FFMPEG mutex");
            #endif
            *pMutex = new Mutex();
            return 0;
        case AV_LOCK_OBTAIN:
            #ifdef MS_DEBUG_FFMPEG_MUTEX
                LOG(LOG_VERBOSE, "Going to lock FFMPEG mutex");
            #endif
            tMutex = (Mutex*)*pMutex;
            tTmpResult = !tMutex->lock();
            #ifdef MS_DEBUG_FFMPEG_MUTEX
                LOG(LOG_VERBOSE, "FFMPEG mutex locked");
            #endif
            return tTmpResult;
        case AV_LOCK_RELEASE:
            #ifdef MS_DEBUG_FFMPEG_MUTEX
                LOG(LOG_VERBOSE, "Going to unlock FFMPEG mutex");
            #endif
            tMutex = (Mutex*)*pMutex;
            tTmpResult = !tMutex->unlock();
            #ifdef MS_DEBUG_FFMPEG_MUTEX
                LOG(LOG_VERBOSE, "FFMPEG mutex unlock");
            #endif
            return tTmpResult;
        case AV_LOCK_DESTROY:
            #ifdef MS_DEBUG_FFMPEG_MUTEX
                LOG(LOG_VERBOSE, "Destroying FFMPEG mutex");
            #endif
            tMutex = (Mutex*)*pMutex;
            delete tMutex;
            return 0;
        default:
        	LOGEX(MediaSource, LOG_ERROR, "We should never reach this point. Unknown mutex operation requested by ffmpeg library.");
        	break;
    }
    return 1;
}

/*************************************************
 *  GUI name to video codec ID mapping:
 *  ================================
 *        H.261							CODEC_ID_H261
 *        H.263							CODEC_ID_H263
 *        MPEG1							CODEC_ID_MPEG1VIDEO
 *        MPEG2							CODEC_ID_MPEG2VIDEO
 *        H.263+						CODEC_ID_H263P+
 *        H.264							CODEC_ID_H264
 *        MPEG4							CODEC_ID_MPEG4
 *        THEORA						CODEC_ID_THEORA
 *        VP8							CODEC_ID_VP8
 *
 *
 *  GUI name to audio codec ID mapping:
 *  ================================
 *        G711 A-law					CODEC_ID_PCM_MULAW
 *        GSM							CODEC_ID_GSM
 *        G711 �-law					CODEC_ID_PCM_ALAW
 *        G722 adpcm					CODEC_ID_ADPCM_G722
 *        PCM16							CODEC_ID_PCM_S16BE
 *        MP3							CODEC_ID_MP3
 *        AAC							CODEC_ID_AAC
 *        AMR							CODEC_ID_AMR_NB
 *
 ****************************************************/
enum AVCodecID MediaSource::GetCodecIDFromGuiName(std::string pName)
{
    enum AVCodecID tResult = (GetMediaType() == MEDIA_AUDIO) ? AV_CODEC_ID_MP3 : AV_CODEC_ID_H261;

    /* video */
    if (pName == "H.261")
        tResult = AV_CODEC_ID_H261;
    if (pName == "H.263")
        tResult = AV_CODEC_ID_H263;
    if (pName == "MPEG1")
        tResult = AV_CODEC_ID_MPEG1VIDEO;
    if (pName == "MPEG2")
        tResult = AV_CODEC_ID_MPEG2VIDEO;
    if (pName == "H.263+")
        tResult = AV_CODEC_ID_H263P;
    if (pName == "H.264")
        tResult = AV_CODEC_ID_H264;
    if (pName == "MPEG4")
        tResult = AV_CODEC_ID_MPEG4;
    if (pName == "MJPEG")
        tResult = AV_CODEC_ID_MJPEG;
    if (pName == "THEORA")
        tResult = AV_CODEC_ID_THEORA;
    if (pName == "VP8")
        tResult = AV_CODEC_ID_VP8;

    /* audio */
    if ((pName == "G711 �-law") || (pName == "G711 �-law (PCMU)" /*historic*/))
        tResult = AV_CODEC_ID_PCM_MULAW;
    if (pName == "GSM")
        tResult = AV_CODEC_ID_GSM;
    if ((pName == "G711 A-law") || (pName == "G711 A-law (PCMA)" /*historic*/))
        tResult = AV_CODEC_ID_PCM_ALAW;
    if (pName == "G722 adpcm")
        tResult = AV_CODEC_ID_ADPCM_G722;
    if ((pName == "PCM16") || (pName == "PCM_S16_LE" /*historic*/))
        tResult = AV_CODEC_ID_PCM_S16BE;
    if ((pName == "MP3") || (pName == "MP3 (MPA)" /*historic*/))
        tResult = AV_CODEC_ID_MP3;
    if (pName == "AAC")
        tResult = AV_CODEC_ID_AAC;
    if (pName == "AMR")
        tResult = AV_CODEC_ID_AMR_NB;
    if (pName == "AC3")
        tResult = AV_CODEC_ID_AC3;

    //LOG(LOG_VERBOSE, "Translated %s to %d", pName.c_str(), tResult);

    return tResult;
}

string MediaSource::GetGuiNameFromCodecID(enum AVCodecID pCodecId)
{
    string tResult = "";

    switch(pCodecId)
    {
    	/* video */
    	case AV_CODEC_ID_H261:
    			tResult = "H.261";
    			break;
    	case AV_CODEC_ID_H263:
    			tResult = "H.263";
    			break;
    	case AV_CODEC_ID_MPEG1VIDEO:
    			tResult = "MPEG1";
    			break;
        case AV_CODEC_ID_MPEG2VIDEO:
    			tResult = "MPEG2";
    			break;
        case AV_CODEC_ID_H263P:
    			tResult = "H.263+";
    			break;
        case AV_CODEC_ID_H264:
    			tResult = "H.264";
    			break;
        case AV_CODEC_ID_MPEG4:
    			tResult = "MPEG4";
    			break;
        case AV_CODEC_ID_MJPEG:
    			tResult = "MJPEG";
    			break;
        case AV_CODEC_ID_THEORA:
    			tResult = "THEORA";
    			break;
        case AV_CODEC_ID_VP8:
    			tResult = "VP8";
    			break;

		/* audio */
        case AV_CODEC_ID_PCM_MULAW:
    			tResult = "G711 �-law";
    			break;
        case AV_CODEC_ID_GSM:
    			tResult = "GSM";
    			break;
        case AV_CODEC_ID_PCM_ALAW:
    			tResult = "G711 A-law";
    			break;
        case AV_CODEC_ID_ADPCM_G722:
    			tResult = "G722 adpcm";
    			break;
        case AV_CODEC_ID_PCM_S16BE:
    			tResult = "PCM16";
    			break;
        case AV_CODEC_ID_MP3:
    			tResult = "MP3";
    			break;
        case AV_CODEC_ID_AAC:
    			tResult = "AAC";
    			break;
        case AV_CODEC_ID_AMR_NB:
    			tResult = "AMR";
    			break;
        case AV_CODEC_ID_AC3:
    			tResult = "AC3";
    			break;

        default:
        	//LOGEX(MediaSource, LOG_WARN, "Detected unsupported codec %d", pCodecId);
        	break;
    }

    //LOGEX(MediaSource, LOG_VERBOSE, "Translated %s to format %s", pName.c_str(), tResult.c_str());

    return tResult;
}

/*************************************************
 *  video codec ID to format mapping:
 *  ================================
 *        AV_CODEC_ID_H261					h261
 *        AV_CODEC_ID_H263					h263
 *        AV_CODEC_ID_MPEG1VIDEO			mpeg1video
 *        AV_CODEC_ID_MPEG2VIDEO			mpeg2video
 *        AV_CODEC_ID_H263P+				h263 // same like H263
 *        AV_CODEC_ID_H264					h264
 *        AV_CODEC_ID_MPEG4				m4v
 *        AV_CODEC_ID_MJPEG				mjpeg
 *        AV_CODEC_ID_THEORA				ogg
 *        AV_CODEC_ID_VP8					webm
 *
 *
 *  audio codec ID to format mapping:
 *  ================================
 *        AV_CODEC_ID_PCM_MULAW			mulaw
 *        AV_CODEC_ID_GSM					libgsm
 *        AV_CODEC_ID_PCM_ALAW				alaw
 *        AV_CODEC_ID_ADPCM_G722			g722
 *        AV_CODEC_ID_PCM_S16BE			s16be
 *        AV_CODEC_ID_MP3					mp3
 *        AV_CODEC_ID_AAC					aac
 *        AV_CODEC_ID_AMR_NB				amr
 *
 ****************************************************/
string MediaSource::GetFormatName(enum AVCodecID pCodecId)
{
    string tResult = "";

    switch(pCodecId)
    {
    	/* video */
    	case AV_CODEC_ID_H261:
    			tResult = "h261";
    			break;
    	case AV_CODEC_ID_H263:
    			tResult = "h263";
    			break;
    	case AV_CODEC_ID_MPEG1VIDEO:
    			tResult = "mpeg1video";
    			break;
        case AV_CODEC_ID_MPEG2VIDEO:
    			tResult = "mpeg2video";
    			break;
        case AV_CODEC_ID_H263P:
    			tResult = "h263"; // ffmpeg has no separate h263+ format
    			break;
        case AV_CODEC_ID_H264:
    			tResult = "h264";
    			break;
        case AV_CODEC_ID_MPEG4:
    			tResult = "m4v";
    			break;
        case AV_CODEC_ID_MJPEG:
    			tResult = "mjpeg";
    			break;
        case AV_CODEC_ID_THEORA:
    			tResult = "ogg";
    			break;
        case AV_CODEC_ID_VP8:
    			tResult = "webm";
    			break;

		/* audio */
        case AV_CODEC_ID_PCM_MULAW:
    			tResult = "mulaw";
    			break;
        case AV_CODEC_ID_GSM:
    			tResult = "gsm";
    			break;
        case AV_CODEC_ID_PCM_ALAW:
    			tResult = "alaw";
    			break;
        case AV_CODEC_ID_ADPCM_G722:
    			tResult = "g722";
    			break;
        case AV_CODEC_ID_PCM_S16BE:
    			tResult = "s16be";
    			break;
        case AV_CODEC_ID_MP3:
    			tResult = "mp3";
    			break;
        case AV_CODEC_ID_AAC:
    			tResult = "aac";
    			break;
        case AV_CODEC_ID_AMR_NB:
    			tResult = "amr";
    			break;
        case AV_CODEC_ID_AC3:
    			tResult = "ac3";
    			break;

        default:
        	LOGEX(MediaSource, LOG_WARN, "Detected unsupported %s codec %d", GetMediaTypeStr().c_str(), pCodecId);
        	break;
    }

    //LOGEX(MediaSource, LOG_VERBOSE, "Translated codec id %d to format %s", pCodecId, tResult.c_str());

    return tResult;
}

int MediaSource::AudioQuality2BitRate(int pQuality)
{
    int tResult = 128000;

    switch(pQuality)
    {
        case 100:
                tResult = 256000;
                break;
        case 90:
                tResult = 128000;
                break;
        case 80:
                tResult = 96000;
                break;
        case 70:
                tResult = 64000;
                break;
        case 60:
                tResult = 56000;
                break;
        case 50:
                tResult = 48000;
                break;
        case 40:
                tResult = 32000;
                break;
        case 30:
                tResult = 24000;
                break;
        case 20:
                tResult = 16000;
                break;
        case 10:
                tResult = 8000;
                break;
    }

    return tResult;
}

int MediaSource::GetOutputSampleRate()
{
    return mOutputAudioSampleRate;
}

int MediaSource::GetOutputChannels()
{
	return mOutputAudioChannels;
}

int MediaSource::GetInputSampleRate()
{
    return mInputAudioSampleRate;
}

int MediaSource::GetInputChannels()
{
	return mInputAudioChannels;
}

string MediaSource::GetInputFormatStr()
{
    return av_get_sample_fmt_name(mInputAudioFormat);
}

int MediaSource::GetInputBitRate()
{
	return mInputBitRate;
}

bool MediaSource::HasVariableVideoOutputFrameRate()
{
	return false;
}

bool MediaSource::IsSeeking()
{
	return false;
}

AVFrame *MediaSource::AllocFrame()
{
    AVFrame *tResult = avcodec_alloc_frame();
    if (tResult != NULL)
        avcodec_get_frame_defaults(tResult);
    return tResult;
}

int MediaSource::FillFrame(AVFrame *pFrame, void *pData, enum PixelFormat pPixFormat, int pWidth, int pHeight)
{
	return avpicture_fill((AVPicture *)pFrame, (uint8_t *)pData, pPixFormat, pWidth, pHeight);
}

void MediaSource::VideoFormat2Resolution(VideoFormat pFormat, int& pX, int& pY)
{
    switch(pFormat)
    {

        case SQCIF:      /*      128 �  96       */
                    pX = 128;
                    pY = 96;
                    break;
        case QCIF:       /*      176 � 144       */
                    pX = 176;
                    pY = 144;
                    break;
        case CIF:        /*      352 � 288       */
                    pX = 352;
                    pY = 288;
                    break;
        case CIF4:       /*      704 � 576       */
                    pX = 704;
                    pY = 576;
                    break;
        case DVD:        /*      720 � 576       */
                    pX = 720;
                    pY = 576;
                    break;
        case CIF9:       /*     1056 � 864       */
                    pX = 1056;
                    pY = 864;
                    break;
        case EDTV:       /*     1280 � 720       */
                    pX = 1280;
                    pY = 720;
                    break;
        case CIF16:      /*     1408 � 1152      */
                    pX = 1408;
                    pY = 1152;
                    break;
        case HDTV:       /*     1920 � 1080       */
                    pX = 1920;
                    pY = 1080;
                    break;
    }
}

void MediaSource::VideoString2Resolution(string pString, int& pX, int& pY)
{
    pX = 352;
    pY = 288;

    if (pString == "auto")
    {
        pX = -1;
        pY = -1;
    }
    if (pString == "128 * 96")
    {
        pX = 128;
        pY = 96;
    }
    if (pString == "176 * 144")
    {
        pX = 176;
        pY = 144;
    }
    if (pString == "352 * 288")
    {
        pX = 352;
        pY = 288;
    }
    if (pString == "704 * 576")
    {
        pX = 704;
        pY = 576;
    }
    if (pString == "720 * 576")
    {
        pX = 720;
        pY = 576;
    }
    if (pString == "1056 * 864")
    {
        pX = 1056;
        pY = 864;
    }
    if (pString == "1280 * 720")
    {
        pX = 1280;
        pY = 720;
    }
    if (pString == "1408 * 1152")
    {
        pX = 1408;
        pY = 1152;
    }
    if (pString == "1920 * 1080")
    {
        pX = 1920;
        pY = 1080;
    }
    //LOGEX(MediaSource, LOG_VERBOSE, "Derived video resolution: %d*%d", pX, pY);
}

bool MediaSource::SupportsDecoderFrameStatistics()
{
    return false;
}

int64_t MediaSource::DecodedIFrames()
{
    return mDecodedIFrames;
}

int64_t MediaSource::DecodedPFrames()
{
    return mDecodedPFrames;
}

int64_t MediaSource::DecodedBFrames()
{
    return mDecodedBFrames;
}

int64_t MediaSource::DecodedSFrames()
{
    return mDecodedSFrames;
}

int64_t MediaSource::DecodedSIFrames()
{
    return mDecodedSIFrames;
}

int64_t MediaSource::DecodedSPFrames()
{
    return mDecodedSPFrames;
}

int64_t MediaSource::DecodedBIFrames()
{
    return mDecodedBIFrames;
}

int64_t MediaSource::GetEndToEndDelay()
{
    return mEndToEndDelay;
}

float MediaSource::GetFrameBufferPreBufferingTime()
{
    return mDecoderFramePreBufferTime;
}

void MediaSource::SetFrameBufferPreBufferingTime(float pTime)
{
    if (mDecoderFramePreBufferTime != pTime)
    {
        LOG(LOG_VERBOSE, "Setting frame pre-buffer time to %.2f", pTime);
        mDecoderFramePreBufferTime = pTime;
    }
}

float MediaSource::GetFrameBufferTime()
{
	return mDecoderFrameBufferTime;
}

float MediaSource::GetFrameBufferTimeMax()
{
    return mDecoderFrameBufferTimeMax;
}

int MediaSource::GetFrameBufferCounter()
{
	return 0;
}

int MediaSource::GetFrameBufferSize()
{
    return 0;
}

void MediaSource::SetPreBufferingActivation(bool pActive)
{
    LOG(LOG_VERBOSE, "Setting pre-buffering for %s source to: %d", GetMediaTypeStr().c_str(), pActive);
    if (!pActive)
        mDecoderFramePreBufferTime = 0;
}

void MediaSource::SetPreBufferingAutoRestartActivation(bool pActive)
{
    LOG(LOG_VERBOSE, "Setting pre-buffering auto. restart for %s source to: %d", GetMediaTypeStr().c_str(), pActive);
    mDecoderFramePreBufferingAutoRestart = pActive;
}

int MediaSource::GetDecoderOutputFrameDelay()
{
    return mDecoderOutputFrameDelay;
}

void MediaSource::CalibrateRTGrabbing()
{
    LOG(LOG_WARN, "Called CalibrateRTGrabbing()");
}

bool MediaSource::WaitForRTGrabbing()
{
    // ##################################################################
    // ### RT grabbing to match set fps rate
    // ### we use a timestamp history to provide a more stable fps rate
    // ##################################################################
    if (mRTGrabbingFrameTimestamps.size() > 0)
    {
        // calculate the time which corresponds to the request FPS
        int64_t tTimePerFrame = 1000000 / mInputFrameRate; // in us

        // calculate the time difference for the current frame in relation to the first timestamp in the history
        int64_t tTimeDiffForHistory = tTimePerFrame * mRTGrabbingFrameTimestamps.size();

        // calculate the desired play-out time for the current frame by using the first timestamp in the history as time reference
        int64_t tDesiredPlayOutTime = mRTGrabbingFrameTimestamps.front() + tTimeDiffForHistory;

        // get the time since last successful grabbing
        int64_t tWaitingTine = tDesiredPlayOutTime - Time::GetTimeStamp(); // in us

        if (tWaitingTine > 0)
        {// skip capturing when we are too fast
            #ifdef MSL_DEBUG_PACKETS
                LOG(LOG_VERBOSE, "Logo capturing delayed by %"PRId64" ms for frame %d", tWaitingTine / 1000, mFrameNumber);
            #endif
            Thread::Suspend(tWaitingTine);
        }else
        {// no waiting
            //LOG(LOG_VERBOSE, "No waiting for frame %d (time=%"PRId64")", mFrameNumber, tWaitingTine);
        }

        // limit history of timestamps
        while (mRTGrabbingFrameTimestamps.size() > MEDIA_SOURCE_RT_GRABBING_TIMESTAMP_HISTORY)
            mRTGrabbingFrameTimestamps.pop_front();
    }

    // store current timestamp
    mRTGrabbingFrameTimestamps.push_back(Time::GetTimeStamp());

    return true;
}

void MediaSource::DoSetVideoGrabResolution(int pResX, int pResY)
{
    CloseGrabDevice();
    OpenVideoGrabDevice(pResX, pResY, GetInputFrameRate());
}

void MediaSource::SetVideoGrabResolution(int pResX, int pResY)
{
	if(!mRecording)
	{
		if ((pResX != mTargetResX) || (pResY != mTargetResY))
		{
			LOG(LOG_VERBOSE, "Setting video grabbing resolution to %d * %d", pResX, pResY);

			mTargetResX = pResX;
			mTargetResY = pResY;
			if ((mMediaType == MEDIA_VIDEO) && (mMediaSourceOpened))
			{
				// lock grabbing
				mGrabMutex.lock();

				DoSetVideoGrabResolution(mTargetResX, mTargetResY);

				// unlock grabbing
				mGrabMutex.unlock();
			}
		}
	}else
	{
		LOG(LOG_ERROR, "Can't change video resolution if recording is activated");
	}
}

void MediaSource::GetVideoGrabResolution(int &pResX, int &pResY)
{
    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return;
    }

    pResX = mTargetResX;
    pResY = mTargetResY;
}

void MediaSource::GetVideoSourceResolution(int &pResX, int &pResY)
{
    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return;
    }

    pResX = mSourceResX;
    pResY = mSourceResY;
}

GrabResolutions MediaSource::GetSupportedVideoGrabResolutions()
{
    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        GrabResolutions tRes;
        return tRes;
    }

    return mSupportedVideoFormats;
}

void MediaSource::StopGrabbing()
{
    mGrabbingStopped = true;
}

bool MediaSource::Reset(enum MediaType pMediaType)
{
	bool tResult = false;

    // HINT: closing the grab device resets the media type!
    int tMediaType = (pMediaType == MEDIA_UNKNOWN) ? mMediaType : pMediaType;

    StopGrabbing();

    // lock grabbing
    mGrabMutex.lock();

    CloseGrabDevice();

    // restart media source, assuming that the last start of the media source was successful
    // otherwise a manual call to Open(Video/Audio)GrabDevice besides this reset function is need
    switch(tMediaType)
    {
        case MEDIA_VIDEO:
            tResult = OpenVideoGrabDevice(mSourceResX, mSourceResY, GetInputFrameRate());
            break;
        case MEDIA_AUDIO:
            tResult = OpenAudioGrabDevice(mOutputAudioSampleRate, mOutputAudioChannels);
            break;
        case MEDIA_UNKNOWN:
            //LOG(LOG_ERROR, "Media type unknown");
            break;
    }

    // unlock grabbing
    mGrabMutex.unlock();

    mFrameNumber = 0;

    return tResult;
}

enum AVCodecID MediaSource::GetSourceCodec()
{
    return mSourceCodecId;
}

string MediaSource::GetSourceCodecStr()
{
	string tResult = "unknown";

    if (mMediaSourceOpened)
        if (mCodecContext != NULL)
            if (mCodecContext->codec != NULL)
                if (mCodecContext->codec->name != NULL)
                {
                	string tName = GetGuiNameFromCodecID(mCodecContext->codec->id);
                	if (tName != "")
                		tResult = tName;
                	else
                		tResult = string(mCodecContext->codec->name);
                }

    return tResult;
}

string MediaSource::GetSourceCodecDescription()
{
	string tResult = "unknown";

    if (mMediaSourceOpened)
        if (mCodecContext != NULL)
            if (mCodecContext->codec != NULL)
                if (mCodecContext->codec->long_name != NULL)
                    tResult = string(mCodecContext->codec->long_name);

    return tResult;
}

bool MediaSource::SetInputStreamPreferences(std::string pStreamCodec, bool pRtpActivated, bool pDoReset)
{
    LOG(LOG_VERBOSE, "This is only the dummy SetInputPreferences() function");
    return false;
}

void MediaSource::ResetPacketStatistic()
{
    PacketStatistic::ResetPacketStatistic();
    mChunkDropCounter = 0;
}

int MediaSource::GetChunkDropCounter()
{
    return mChunkDropCounter;
}

int MediaSource::GetFragmentBufferCounter()
{
    return 0;
}

int MediaSource::GetFragmentBufferSize()
{
    return 0;
}

MediaSinkNet* MediaSource::RegisterMediaSink(string pTarget, Requirements *pTransportRequirements, bool pRtpActivation, int pMaxFps)
{
    MediaSinks::iterator tIt;
    bool tFound = false;
    MediaSinkNet *tResult = NULL;
    string tId = pTarget + "[" + pTransportRequirements->getDescription() + "]" + (pRtpActivation ? "(RTP)" : "");

    if (pTarget == "")
    {
        LOG(LOG_ERROR, "Sink is ignored because its target is undefined");
        return NULL;
    }

    LOG(LOG_VERBOSE, "Registering NAPI based media sink: %s (Requirements: %s)", pTarget.c_str(), pTransportRequirements->getDescription().c_str());

    // lock
    mMediaSinksMutex.lock();

    for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
    {
        if ((*tIt)->GetId().find(tId) != string::npos)
        {
            LOG(LOG_WARN, "Sink already registered");
            tFound = true;
            break;
        }
    }

    if (!tFound)
    {
        MediaSinkNet *tMediaSinkNet = new MediaSinkNet(pTarget, pTransportRequirements, (mMediaType == MEDIA_VIDEO) ? MEDIA_SINK_VIDEO : MEDIA_SINK_AUDIO, pRtpActivation);
        tMediaSinkNet->SetMaxFps(pMaxFps);
        mMediaSinks.push_back(tMediaSinkNet);
        tResult = tMediaSinkNet;
    }

    // unlock
    mMediaSinksMutex.unlock();

    return tResult;
}

bool MediaSource::UnregisterMediaSink(string pTarget, Requirements *pTransportRequirements, bool pAutoDelete)
{
    bool tResult = false;
    MediaSinks::iterator tIt;
    string tId = pTarget + "[" + pTransportRequirements->getDescription() + "]";

    if (pTarget == "")
        return false;

    LOG(LOG_VERBOSE, "Unregistering NAPI based media sink: %s (Requirements: %s)", pTarget.c_str(), pTransportRequirements->getDescription().c_str());

    // lock
    mMediaSinksMutex.lock();

    for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
    {
        if ((*tIt)->GetId().find(tId) != string::npos)
        {
            LOG(LOG_VERBOSE, "Found registered sink");

            tResult = true;
            // free memory of media sink object
            if (pAutoDelete)
            {
                delete (*tIt);
                LOG(LOG_VERBOSE, "..deleted");
            }
            // remove registration of media sink object
            mMediaSinks.erase(tIt);
            LOG(LOG_VERBOSE, "..unregistered");
            break;
        }
    }

    // unlock
    mMediaSinksMutex.unlock();

    return tResult;
}

MediaSinkNet* MediaSource::RegisterMediaSink(string pTargetHost, unsigned int pTargetPort, Socket* pSocket, bool pRtpActivation, int pMaxFps)
{
    MediaSinks::iterator tIt;
    bool tFound = false;
    MediaSinkNet *tResult = NULL;
    string tId = MediaSinkNet::CreateId(pTargetHost, toString(pTargetPort), pSocket->GetTransportType(), pRtpActivation);

    if ((pTargetHost == "") || (pTargetPort == 0))
    {
        LOG(LOG_ERROR, "Sink is ignored because its target is undefined");
        return NULL;
    }

    LOG(LOG_VERBOSE, "Registering Berkeley sockets based media sink: %s<%d>", pTargetHost.c_str(), pTargetPort);

    // lock
    mMediaSinksMutex.lock();

    for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
    {
        if ((*tIt)->GetId().find(tId) != string::npos)
        {
            LOG(LOG_WARN, "Sink already registered");
            tFound = true;
            break;
        }
    }

    if (!tFound)
    {
        MediaSinkNet *tMediaSinkNet = new MediaSinkNet(pTargetHost, pTargetPort, pSocket, (mMediaType == MEDIA_VIDEO) ? MEDIA_SINK_VIDEO : MEDIA_SINK_AUDIO, pRtpActivation);
        tMediaSinkNet->SetMaxFps(pMaxFps);
        mMediaSinks.push_back(tMediaSinkNet);
        tResult = tMediaSinkNet;
    }

    // unlock
    mMediaSinksMutex.unlock();

    return tResult;
}

bool MediaSource::UnregisterMediaSink(string pTargetHost, unsigned int pTargetPort, bool pAutoDelete)
{
    bool tResult = false;
    MediaSinks::iterator tIt;
    string tId = MediaSinkNet::CreateId(pTargetHost, toString(pTargetPort));

    if ((pTargetHost == "") || (pTargetPort == 0))
        return false;

    LOG(LOG_VERBOSE, "Unregistering net based media sink: %s<%d>", pTargetHost.c_str(), pTargetPort);

    // lock
    mMediaSinksMutex.lock();

    for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
    {
        if ((*tIt)->GetId().find(tId) != string::npos)
        {
            LOG(LOG_VERBOSE, "Found registered sink");

            tResult = true;
            // free memory of media sink object
            if (pAutoDelete)
            {
                delete (*tIt);
                LOG(LOG_VERBOSE, "..deleted");
            }
            // remove registration of media sink object
            mMediaSinks.erase(tIt);
            LOG(LOG_VERBOSE, "..unregistered");
            break;
        }
    }

    // unlock
    mMediaSinksMutex.unlock();

    return tResult;
}

MediaSinkFile* MediaSource::RegisterMediaSink(string pTargetFile)
{
    MediaSinks::iterator tIt;
    bool tFound = false;
    MediaSinkFile *tResult = NULL;
    string tId = pTargetFile;

    if (pTargetFile == "")
    {
        LOG(LOG_ERROR, "Sink is ignored because its id is undefined");
        return NULL;
    }

    LOG(LOG_VERBOSE, "Registering file based media sink: %s", pTargetFile.c_str());

    // lock
    mMediaSinksMutex.lock();

    for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
    {
        if ((*tIt)->GetId() == tId)
        {
            LOG(LOG_WARN, "Sink already registered");
            tFound = true;
            break;
        }
    }

    if (!tFound)
    {
        MediaSinkFile *tMediaSinkFile = new MediaSinkFile(pTargetFile, (mMediaType == MEDIA_VIDEO)?MEDIA_SINK_VIDEO:MEDIA_SINK_AUDIO, mRtpActivated);
        mMediaSinks.push_back(tMediaSinkFile);
        tResult = tMediaSinkFile;
    }

    // unlock
    mMediaSinksMutex.unlock();

    return tResult;
}

bool MediaSource::UnregisterMediaSink(string pTargetFile, bool pAutoDelete)
{
    bool tResult = false;
    MediaSinks::iterator tIt;
    string tId = pTargetFile;

    if (pTargetFile == "")
        return false;

    LOG(LOG_VERBOSE, "Unregistering file based media sink: %s", pTargetFile.c_str());

    // lock
    mMediaSinksMutex.lock();

    for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
    {
        if ((*tIt)->GetId() == tId)
        {
            LOG(LOG_VERBOSE, "Found registered sink");

            tResult = true;
            // free memory of media sink object
            if (pAutoDelete)
            {
                delete (*tIt);
                LOG(LOG_VERBOSE, "..deleted");
            }
            // remove registration of media sink object
            mMediaSinks.erase(tIt);
            LOG(LOG_VERBOSE, "..unregistered");
            break;
        }
    }

    // unlock
    mMediaSinksMutex.unlock();

    return tResult;
}

MediaSink* MediaSource::RegisterMediaSink(MediaSink* pMediaSink)
{
    MediaSinks::iterator tIt;
    bool tFound = false;
    string tId = pMediaSink->GetId();

    if (tId == "")
    {
        LOG(LOG_ERROR, "Sink is ignored because its id is undefined");
        return NULL;
    }

    LOG(LOG_VERBOSE, "Registering media sink: %s", tId.c_str());

    // lock
    mMediaSinksMutex.lock();

    for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
    {
        if ((*tIt)->GetId() == tId)
        {
            LOG(LOG_WARN, "Sink already registered");
            tFound = true;
            break;
        }
    }

    if (!tFound)
        mMediaSinks.push_back(pMediaSink);

    // unlock
    mMediaSinksMutex.unlock();

    return pMediaSink;
}

bool MediaSource::UnregisterMediaSink(MediaSink* pMediaSink, bool pAutoDelete)
{
    bool tResult = false;
    MediaSinks::iterator tIt;
    string tId = pMediaSink->GetId();

    if (tId == "")
        return false;

    LOG(LOG_VERBOSE, "Unregistering media sink: %s", tId.c_str());

    // lock
    mMediaSinksMutex.lock();

    for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
    {
        if ((*tIt)->GetId() == tId)
        {
            LOG(LOG_VERBOSE, "Found registered sink");

            tResult = true;
            // free memory of media sink object
            if (pAutoDelete)
            {
                delete (*tIt);
                LOG(LOG_VERBOSE, "..deleted");
            }
            // remove registration of media sink object
            mMediaSinks.erase(tIt);
            LOG(LOG_VERBOSE, "..unregistered");
            break;
        }
    }

    // unlock
    mMediaSinksMutex.unlock();

    return tResult;
}

bool MediaSource::UnregisterGeneralMediaSink(string pId, bool pAutoDelete)
{
    bool tResult = false;
    MediaSinks::iterator tIt;

    if (pId == "")
        return false;

    LOG(LOG_VERBOSE, "Unregistering media sink: %s", pId.c_str());

    // lock
    mMediaSinksMutex.lock();

    for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
    {
        if ((*tIt)->GetId() == pId)
        {
            LOG(LOG_VERBOSE, "Found registered sink");

            tResult = true;
            // free memory of media sink object
            if (pAutoDelete)
            {
                delete (*tIt);
                LOG(LOG_VERBOSE, "..deleted");
            }
            // remove registration of media sink object
            mMediaSinks.erase(tIt);
            LOG(LOG_VERBOSE, "..unregistered");
            break;
        }
    }

    // unlock
    mMediaSinksMutex.unlock();

    return tResult;
}

vector<string> MediaSource::ListRegisteredMediaSinks()
{
    vector<string> tResult;
    MediaSinks::iterator tIt;

    // lock
    if (mMediaSinksMutex.lock(100))
    {
        if(mMediaSinks.size() > 0)
        {
            for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
            {
                tResult.push_back((*tIt)->GetId());
            }
        }

        // unlock
        mMediaSinksMutex.unlock();
    }else
        LOG(LOG_WARN, "Failed to lock mutex in specified time");

    return tResult;
}

void MediaSource::DeleteAllRegisteredMediaSinks()
{
    MediaSinks::iterator tIt;

    // lock
    mMediaSinksMutex.lock();

    while(mMediaSinks.size())
    {
        for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
        {
            LOG(LOG_VERBOSE, "Deleting registered sink %s", (*tIt)->GetId().c_str());

            // free memory of media sink object
            delete (*tIt);
            LOG(LOG_VERBOSE, "..deleted");

            // remove registration of media sink object
            mMediaSinks.erase(tIt);
            LOG(LOG_VERBOSE, "..unregistered");
            break;
        }
    }

    // unlock
    mMediaSinksMutex.unlock();
}

void MediaSource::SetRtpActivation(bool pState)
{
    mRtpActivated = pState;
}

bool MediaSource::GetRtpActivation()
{
    return mRtpActivated;
}

bool MediaSource::SupportsRelaying()
{
    return false;
}

int MediaSource::GetEncoderBufferedFrames()
{
	return 0;
}

void MediaSource::RelayPacketToMediaSinks(char* pPacketData, unsigned int pPacketSize, bool pIsKeyFrame)
{
    MediaSinks::iterator tIt;

    // lock
    mMediaSinksMutex.lock();

    #ifdef MS_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Relaying packet for %s %s media source to %d media sinks", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), mMediaSinks.size());
    #endif

    if (mMediaSinks.size() > 0)
    {
        for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
        {
            (*tIt)->ProcessPacket(pPacketData, pPacketSize, mFormatContext->streams[0], pIsKeyFrame);
        }
    }

    // unlock
    mMediaSinksMutex.unlock();
}

void MediaSource::RelaySyncTimestampToMediaSinks(int64_t pReferenceNtpTimestamp, int64_t pReferenceFrameTimestamp)
{
    MediaSinks::iterator tIt;

    #ifdef MS_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Update synch. for all media sinks");
    #endif

    // lock
    mMediaSinksMutex.lock();

    if (mMediaSinks.size() > 0)
    {
        for (tIt = mMediaSinks.begin(); tIt != mMediaSinks.end(); tIt++)
        {
            (*tIt)->UpdateSynchronization(pReferenceNtpTimestamp, pReferenceFrameTimestamp);
        }
    }

    // unlock
    mMediaSinksMutex.unlock();
}

bool MediaSource::StartRecording(std::string pSaveFileName, int pSaveFileQuality, bool pRealTime /*TODO: delete this*/)
{
    int                 tResult;
    AVOutputFormat      *tFormat;
    AVCodec             *tCodec;
    AVDictionary        *tOptions = NULL;
    AVCodecID           tSaveFileCodec = AV_CODEC_ID_NONE;

    LOG(LOG_VERBOSE, "Going to open recorder, media type is \"%s\"", GetMediaTypeStr().c_str());

    //find simple file name
	size_t tSize;
	tSize = pSaveFileName.rfind('/');

	// Windows path?
	if (tSize == string::npos)
		tSize = pSaveFileName.rfind('\\');

	// nothing found?
	if (tSize != string::npos)
		tSize++;
	else
		tSize = 0;

	string tSimpleFileName = pSaveFileName.substr(tSize, pSaveFileName.length() - tSize);

    char tAuthor[512] = "HomerMultimedia";
    //char tTitle[512] = "recorded live video";
    char tCopyright[512] = "free for use";
    char tComment[512] = "www.homer-conferencing.com";

    // lock grabbing
    mGrabMutex.lock();

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        LOG(LOG_ERROR, "Tried to start recording while media source is closed");
        return false;
    }

    if (mRecording)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        LOG(LOG_ERROR, "Recording already started");
        return false;
    }

    // allocate new format context
    mRecorderFormatContext = AV_NEW_FORMAT_CONTEXT();

    // find format
    tFormat = AV_GUESS_FORMAT(NULL, pSaveFileName.c_str(), NULL);

    if (tFormat == NULL)
    {
        LOG(LOG_ERROR, "Invalid suggested recorder format");
        // Close the format context
        av_free(mRecorderFormatContext);

        // unlock grabbing
        mGrabMutex.unlock();

        return false;
    }

    // set correct output format
    mRecorderFormatContext->oformat = tFormat;
    // verbose timestamp debugging
    if (LOGGER.GetLogLevel() == LOG_WORLD)
    {
        LOG(LOG_WARN, "Enabling ffmpeg timestamp debugging for recorder");
        mRecorderFormatContext->debug = FF_FDEBUG_TS;
    }

    // set meta data
    HM_av_dict_set(&mRecorderFormatContext->metadata, "author"   , tAuthor);
    HM_av_dict_set(&mRecorderFormatContext->metadata, "comment"  , tComment);
    HM_av_dict_set(&mRecorderFormatContext->metadata, "copyright", tCopyright);
    HM_av_dict_set(&mRecorderFormatContext->metadata, "title"    , tSimpleFileName.c_str());

    // set filename
    sprintf(mRecorderFormatContext->filename, "%s", pSaveFileName.c_str());

    // allocate new stream structure
    LOG(LOG_VERBOSE, "..allocating new recorder stream");
    mRecorderEncoderStream = HM_avformat_new_stream(mRecorderFormatContext, 0);
    mRecorderCodecContext = mRecorderEncoderStream->codec;

    switch(mMediaType)
    {
        case MEDIA_VIDEO:
                {
                    tSaveFileCodec = tFormat->video_codec;
                    mRecorderCodecContext->codec_id = tFormat->video_codec;
                    mRecorderCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;

                    // add some extra parameters depending on the selected codec
                    switch(tFormat->video_codec)
                    {
                        case AV_CODEC_ID_MPEG4:
                                        mRecorderCodecContext->flags |= CODEC_FLAG_4MV | CODEC_FLAG_AC_PRED;
                                        break;
                    }

                    // bit rate
                    int pSaveBitRate = 500000; //TODO: use as separate function parameter
                    mRecorderCodecContext->bit_rate = pSaveBitRate;

                    // resolution
                    mRecorderCodecContext->width = mSourceResX;
                    mRecorderCodecContext->height = mSourceResY;

                    /*
                     * time base: this is the fundamental unit of time (in seconds) in terms
                     * of which frame timestamps are represented. for fixed-FrameRate content,
                     * timebase should be 1/framerate and timestamp increments should be
                     * identically to 1.
                     */
                    mRecorderCodecContext->time_base = (AVRational){100, (int)(GetInputFrameRate() * 100)};
                    mRecorderEncoderStream->time_base = (AVRational){100, (int)(GetInputFrameRate() * 100)};

                    // set i frame distance: GOP = group of pictures
                    mRecorderCodecContext->gop_size = (100 - pSaveFileQuality) / 5; // default is 12
                    mRecorderCodecContext->qmin = 1; // default is 2
                    mRecorderCodecContext->qmax = 2 +(100 - pSaveFileQuality) / 4; // default is 31

                    // set pixel format
                    mRecorderCodecContext->pix_fmt = PIX_FMT_YUV420P;

                    // allocate software scaler context if necessary
                    if (mCodecContext != NULL)
                        mRecorderVideoScalerContext = sws_getContext(mSourceResX, mSourceResY, mCodecContext->pix_fmt, mSourceResX, mSourceResY, mRecorderCodecContext->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
                    else
                    {
                        LOG(LOG_WARN, "Codec context is invalid, pixel format cannot be determined automatically, assuming RGB32 as input");
                        mRecorderVideoScalerContext = sws_getContext(mSourceResX, mSourceResY, PIX_FMT_RGB32, mSourceResX, mSourceResY, mRecorderCodecContext->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
                    }

                    LOG(LOG_VERBOSE, "..allocating final frame memory");
                    if ((mRecorderFinalFrame = AllocFrame()) == NULL)
                        LOG(LOG_ERROR, "Out of memory in avcodec_alloc_frame()");

                    if (avpicture_alloc((AVPicture*)mRecorderFinalFrame, mRecorderCodecContext->pix_fmt, mSourceResX, mSourceResY) < 0)
                        LOG(LOG_ERROR, "Out of video memory in avpicture_alloc()");
                }
                break;
        case MEDIA_AUDIO:
                {
                    tSaveFileCodec = tFormat->audio_codec;
                    mRecorderCodecContext->codec_id = tFormat->audio_codec;
                    mRecorderCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;

                    // bit rate
                    int pSaveBitRate = AudioQuality2BitRate(pSaveFileQuality); //TODO: use as separate function parameter
                    mRecorderCodecContext->bit_rate = pSaveBitRate;

                    // define audio format for recording
                    mRecorderAudioChannels = 2;
                    if (mRecorderCodecContext->codec_id == AV_CODEC_ID_MP3)
                        mRecorderAudioFormat = AV_SAMPLE_FMT_S16P;
                    else
                        mRecorderAudioFormat = AV_SAMPLE_FMT_S16;
                    mRecorderAudioChannelLayout = av_get_default_channel_layout(mRecorderAudioChannels);
                    mRecorderAudioSampleRate = mInputAudioSampleRate;

                    mRecorderCodecContext->channels = mRecorderAudioChannels;
                    mRecorderCodecContext->channel_layout = mRecorderAudioChannelLayout;
                    mRecorderCodecContext->sample_rate = mRecorderAudioSampleRate;
                    mRecorderCodecContext->sample_fmt = mRecorderAudioFormat;
                    mRecorderCodecContext->channel_layout = mRecorderAudioChannelLayout;

                    // create resample context
                    if (mRecorderAudioResampleContext != NULL)
                        LOG(LOG_ERROR, "State of audio recorder resample context inconsistent");
                    LOG(LOG_WARN, "Audio samples with rate of %d Hz and %d channels (format: %s) have to be resampled to %d Hz and %d channels (format: %s)", mInputAudioSampleRate, mInputAudioChannels, av_get_sample_fmt_name(mInputAudioFormat), mRecorderAudioSampleRate, mRecorderAudioChannels, av_get_sample_fmt_name(mRecorderAudioFormat));
                    mRecorderAudioResampleContext = HM_swr_alloc_set_opts(NULL, av_get_default_channel_layout(mRecorderAudioChannels), mRecorderAudioFormat, mRecorderAudioSampleRate, av_get_default_channel_layout(mInputAudioChannels), mInputAudioFormat, mInputAudioSampleRate, 0, NULL);
                    if (mRecorderAudioResampleContext != NULL)
                    {// everything okay, we have to init the context
                        int tRes = 0;
                        if ((tRes = HM_swr_init(mRecorderAudioResampleContext)) < 0)
                        {
                            LOG(LOG_ERROR, "Couldn't initialize resample context because \"%s\"(%d)", strerror(AVUNERROR(tRes)), tRes);
                            return false;
                        }
                    }else
                    {
                        LOG(LOG_ERROR, "Failed to create audio-resample context");
                    }

                    // resample buffer
                    LOG(LOG_VERBOSE, "..allocating audio resample memory");
                    if (mRecorderResampleBuffer != NULL)
                        LOG(LOG_ERROR, "Recorder resample buffer of %s source was already allocated", GetSourceTypeStr().c_str());
                    mRecorderResampleBuffer = (char*)malloc(MEDIA_SOURCE_SAMPLE_BUFFER_PER_CHANNEL * 32 + FF_INPUT_BUFFER_PADDING_SIZE);

                    LOG(LOG_VERBOSE, "..allocating final frame memory");
                    if ((mRecorderFinalFrame = AllocFrame()) == NULL)
                        LOG(LOG_ERROR, "Out of memory in avcodec_alloc_frame()");

                    if (av_sample_fmt_is_planar(mRecorderAudioFormat))
                    {// planar audio buffering
                        // init fifo buffer per channel
                        for (int i = 0; i < mRecorderAudioChannels; i++)
                            mRecorderResampleFifo[i] = HM_av_fifo_alloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
                    }else
                    {// one interleaved audio buffer
                        mRecorderResampleFifo[0] = HM_av_fifo_alloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
                    }

                }
                break;
        default:
        case MEDIA_UNKNOWN:
                LOG(LOG_ERROR, "Media type unknown");
                break;
    }

    // activate ffmpeg internal fps emulation
    //mRecorderCodecContext->rate_emu = 1;

    // some formats want stream headers to be separate
    if(mRecorderFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
        mRecorderCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

    // allow ffmpeg its speedup tricks
    mRecorderCodecContext->flags2 |= CODEC_FLAG2_FAST;

    // Dump information about device file
    av_dump_format(mRecorderFormatContext, 0, ("MediaSource recorder (" + GetMediaTypeStr() + ")").c_str(), true);

    // find the encoder for the video stream
    if ((tCodec = avcodec_find_encoder(tSaveFileCodec)) == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find a fitting %s codec", GetMediaTypeStr().c_str());

        HM_avformat_close_input(mRecorderFormatContext);

        // unlock grabbing
        mGrabMutex.unlock();

        return false;
    }

    #ifdef MEDIA_SOURCE_RECORDER_MULTI_THREADED_VIDEO_ENCODING
        if (tCodec->capabilities & (CODEC_CAP_FRAME_THREADS | CODEC_CAP_SLICE_THREADS))
        {// threading supported
            // active multi-threading per default for the video encoding: leave two cpus for concurrent tasks (video grabbing/decoding, audio tasks)
            av_dict_set(&tOptions, "threads", "auto", 0);

            // trigger MT usage during video encoding
            int tThreadCount = System::GetMachineCores() - 2;
            if (tThreadCount > 1)
                mRecorderCodecContext->thread_count = tThreadCount;
        }else
        {// threading not supported
            LOG(LOG_WARN, "Multi-threading not supported for %s codec %s", GetMediaTypeStr().c_str(), tCodec->name);
        }
    #endif

    // open codec
    LOG(LOG_VERBOSE, "..opening %s codec", GetMediaTypeStr().c_str());
    if ((tResult = HM_avcodec_open(mRecorderCodecContext, tCodec, NULL)) < 0)
    {
        LOG(LOG_WARN, "Couldn't open %s codec because \"%s\". Will try to open the video open codec without options and with disabled MT..", GetMediaTypeStr().c_str(), strerror(AVUNERROR(tResult)));

        // maybe the encoder doesn't support multi-threading?
        mRecorderCodecContext->thread_count = 1;
        AVDictionary *tNullOptions = NULL;
        if ((tResult = HM_avcodec_open(mRecorderCodecContext, tCodec, &tNullOptions)) < 0)
        {
            LOG(LOG_ERROR, "Couldn't open %s codec because \"%s\".", GetMediaTypeStr().c_str(), strerror(AVUNERROR(tResult)));

            HM_avformat_close_input(mRecorderFormatContext);

            // unlock grabbing
            mGrabMutex.unlock();

            return false;
        }
    }

    // open the output file, if needed
    if (!(tFormat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&mRecorderFormatContext->pb, pSaveFileName.c_str(), AVIO_FLAG_WRITE) < 0)
        {
            LOG(LOG_ERROR, "Could not open \"%s\"\n", pSaveFileName.c_str());

            HM_avformat_close_input(mRecorderFormatContext);

            // unlock grabbing
            mGrabMutex.unlock();

            return false;
        }
    }

    // make sure we have a defined frame size
    if (mRecorderCodecContext->frame_size < 32)
    {
        LOG(LOG_WARN, "Found invalid frame size %d, setting to %d", mRecorderCodecContext->frame_size, MEDIA_SOURCE_SAMPLES_PER_BUFFER);
        mRecorderCodecContext->frame_size = MEDIA_SOURCE_SAMPLES_PER_BUFFER;
    }

    // we have to delay the following to this point because "mRecorderCodecContext->frame_size" access
    if (mMediaType == MEDIA_AUDIO)
    {
        // planes indexing resample buffer
        int InputFrameSize = (mCodecContext != NULL) ? (mCodecContext->frame_size > 0 ? mCodecContext->frame_size : MEDIA_SOURCE_SAMPLES_PER_BUFFER) : MEDIA_SOURCE_SAMPLES_PER_BUFFER;
        LOG(LOG_VERBOSE, "..assigning planes memory for indexing resample memory with frame size: %d", InputFrameSize /* we use the source frame size! */);
        memset(mRecorderResampleBufferPlanes, 0, sizeof(mRecorderResampleBufferPlanes));
        if (HM_av_samples_fill_arrays(&mRecorderResampleBufferPlanes[0], NULL, (uint8_t *)mRecorderResampleBuffer, mRecorderAudioChannels, InputFrameSize /* we use the source frame size! */, mRecorderAudioFormat, 1) < 0)
        {
            LOG(LOG_ERROR, "Could not fill the audio plane pointer array");
        }
        //AVCODEC_MAX_AUDIO_FRAME_SIZE / (av_get_bytes_per_sample(mRecorderAudioFormat) * mRecorderAudioChannels) /* amount of possible output samples */;
        LOG(LOG_VERBOSE, "Resampling buffer is at: %p", mRecorderResampleBuffer);
        for(int i = 0; i < mRecorderAudioChannels; i++)
        {
            LOG(LOG_VERBOSE, "Plane %d index points to: %p (diff: %u)", i, mRecorderResampleBufferPlanes[i], (i > 0) ? (mRecorderResampleBufferPlanes[i] - mRecorderResampleBufferPlanes[i - 1]) : 0);
        }
        LOG(LOG_VERBOSE, "Recorder audio format is planar: %d", av_sample_fmt_is_planar(mRecorderAudioFormat));
    }

    // allocate streams private data buffer and write the streams header, if any
    avformat_write_header(mRecorderFormatContext, NULL);

    LOG(LOG_INFO, "%s recorder opened...", GetMediaTypeStr().c_str());

    LOG(LOG_INFO, "    ..selected recording quality: %d%", pSaveFileQuality);

    LOG(LOG_INFO, "    ..codec name: %s", mRecorderCodecContext->codec->name);
    LOG(LOG_INFO, "    ..codec long name: %s", mRecorderCodecContext->codec->long_name);
    LOG(LOG_INFO, "    ..codec flags: 0x%x", mRecorderCodecContext->flags);
    LOG(LOG_INFO, "    ..codec time_base: %d/%d", mRecorderCodecContext->time_base.num, mRecorderCodecContext->time_base.den);
    LOG(LOG_INFO, "    ..stream rfps: %d/%d", mRecorderEncoderStream->r_frame_rate.num, mRecorderEncoderStream->r_frame_rate.den);
    LOG(LOG_INFO, "    ..stream time_base: %d/%d", mRecorderEncoderStream->time_base.num, mRecorderEncoderStream->time_base.den);
    LOG(LOG_INFO, "    ..stream codec time_base: %d/%d", mRecorderEncoderStream->codec->time_base.num, mRecorderEncoderStream->codec->time_base.den);
    LOG(LOG_INFO, "    ..bit rate: %d", mRecorderCodecContext->bit_rate);
    LOG(LOG_INFO, "    ..desired device: %s", mDesiredDevice.c_str());
    LOG(LOG_INFO, "    ..current device: %s", mCurrentDevice.c_str());
    LOG(LOG_INFO, "    ..qmin: %d", mRecorderCodecContext->qmin);
    LOG(LOG_INFO, "    ..qmax: %d", mRecorderCodecContext->qmax);
    LOG(LOG_INFO, "    ..codec caps: 0x%x", mRecorderCodecContext->codec->capabilities);
    LOG(LOG_INFO, "    ..MT count: %d", mRecorderCodecContext->thread_count);
    LOG(LOG_INFO, "    ..MT method: %d", mRecorderCodecContext->thread_type);
    LOG(LOG_INFO, "    ..frame size: %d", mRecorderCodecContext->frame_size);
    LOG(LOG_INFO, "    ..duration: %.2f frames", mNumberOfFrames);
    LOG(LOG_INFO, "    ..stream context duration: %"PRId64" frames, %.0f seconds, format context duration: %"PRId64", nr. of frames: %"PRId64"", mRecorderEncoderStream->duration, (float)mRecorderEncoderStream->duration / GetInputFrameRate(), mRecorderFormatContext->duration, mRecorderEncoderStream->nb_frames);
    switch(mMediaType)
    {
        case MEDIA_VIDEO:
            LOG(LOG_INFO, "    ..source resolution: %d * %d", mRecorderCodecContext->width, mRecorderCodecContext->height);
            LOG(LOG_INFO, "    ..target resolution: %d * %d", mTargetResX, mTargetResY);
            LOG(LOG_INFO, "    ..i-frame distance: %d", mRecorderCodecContext->gop_size);
            LOG(LOG_INFO, "    ..mpeg quant: %d", mRecorderCodecContext->mpeg_quant);
            LOG(LOG_INFO, "    ..pixel format: %d", (int)mRecorderCodecContext->pix_fmt);
            break;
        case MEDIA_AUDIO:
            LOG(LOG_INFO, "    ..sample rate: %d", mRecorderCodecContext->sample_rate);
            LOG(LOG_INFO, "    ..channels: %d", mRecorderCodecContext->channels);
            LOG(LOG_INFO, "    ..sample format: %s", av_get_sample_fmt_name(mRecorderCodecContext->sample_fmt));
            LOG(LOG_INFO, "Fifo opened...");
            LOG(LOG_INFO, "    ..available FIFO %d size: %d bytes", 0, av_fifo_space(mRecorderResampleFifo[0]));
            break;
        default:
            LOG(LOG_ERROR, "Media type unknown");
            break;
    }

    // unlock grabbing
    mGrabMutex.unlock();

    mRecorderStartPts = -1;
    mRecorderFrameNumber = 0;
    mRecordingSaveFileName = pSaveFileName;
    mRecording = true;
    mRecorderStart = av_gettime();

    return true;
}

void MediaSource::StopRecording()
{
    if (mRecording)
    {
        LOG(LOG_VERBOSE, "Going to close %s recorder", GetMediaTypeStr().c_str());

        mRecording = false;

        // lock grabbing
        mGrabMutex.lock();

        // write the trailer, if any
        av_write_trailer(mRecorderFormatContext);

        switch(mMediaType)
        {
            case MEDIA_VIDEO:
                    // free the software scaler context
                    sws_freeContext(mRecorderVideoScalerContext);

                    // free the file frame's data buffer
                    avpicture_free((AVPicture*)mRecorderFinalFrame);

                    break;
            case MEDIA_AUDIO:
                    // free fifo buffer
                    if (av_sample_fmt_is_planar(mRecorderAudioFormat))
                    {// planar audio buffering
                        // init fifo buffer
                        for (int i = 0; i < mRecorderAudioChannels; i++)
                            av_fifo_free(mRecorderResampleFifo[i]);
                    }else
                    {// one interleaved audio buffer
                        av_fifo_free(mRecorderResampleFifo[0]);
                    }

                    break;
            default:
            case MEDIA_UNKNOWN:
                    LOG(LOG_ERROR, "Media type unknown");
                    break;
        }

        if (mRecorderFinalFrame != NULL)
        {
            // free the file frame
            av_free(mRecorderFinalFrame);
            mRecorderFinalFrame = NULL;
        }

        // close the codec
        mRecorderEncoderStream->discard = AVDISCARD_ALL;
        avcodec_close(mRecorderCodecContext);

        // free codec and stream 0
        av_freep(&mRecorderEncoderStream->codec);
        av_freep(mRecorderEncoderStream);

        if (!(mRecorderFormatContext->oformat->flags & AVFMT_NOFILE))
        	avio_close(mRecorderFormatContext->pb);

        // close the format context
        av_free(mRecorderFormatContext);

        if (mRecorderAudioResampleContext != NULL)
        {
            HM_swr_free(&mRecorderAudioResampleContext);
            mRecorderAudioResampleContext = NULL;
        }
        if (mRecorderResampleBuffer != NULL)
        {
            free(mRecorderResampleBuffer);
            mRecorderResampleBuffer = NULL;
        }

        // unlock grabbing
        mGrabMutex.unlock();

        LOG(LOG_INFO, "...%s recorder stopped", GetMediaTypeStr().c_str());
    }

    mRecorderStartPts = -1;
}

bool MediaSource::SupportsRecording()
{
	return false;
}

bool MediaSource::IsRecording()
{
	return mRecording;
}

int64_t MediaSource::RecordingTime()
{
    int64_t tResult = 0;

    if (IsRecording())
        tResult = (av_gettime() - mRecorderStart) / 1000 / 1000;

    return tResult;
}

void MediaSource::RecordFrame(AVFrame *pSourceFrame)
{
    AVPacket            tPacketStruc, *tPacket = &tPacketStruc;
    int                 tEncoderResult = 0;
    int64_t             tCurrentPts = 1;
    int                 tFrameFinished = 0;
    bool                tKeyFrame;
    int                 tBufferedFrames;

    if (!mRecording)
    {
        LOG(LOG_ERROR, "Recording not started");
        return;
    }

    #ifdef MS_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Recorder source frame..");
        LOG(LOG_VERBOSE, "      ..key frame: %d", pSourceFrame->key_frame);
        LOG(LOG_VERBOSE, "      ..picture type: %s-frame", GetFrameType(pSourceFrame).c_str());
        LOG(LOG_VERBOSE, "      ..pts: %"PRId64"", pSourceFrame->pts);
        LOG(LOG_VERBOSE, "      ..coded pic number: %d", pSourceFrame->coded_picture_number);
        LOG(LOG_VERBOSE, "      ..display pic number: %d", pSourceFrame->display_picture_number);
    #endif

    switch(mMediaType)
    {
        case MEDIA_VIDEO:
                // #########################################
                // has resolution changed since last call?
                // #########################################
                if ((mSourceResX != mRecorderCodecContext->width) || (mSourceResY != mRecorderCodecContext->height))
                {
                    // free the software scaler context
                    sws_freeContext(mRecorderVideoScalerContext);

                    // set grabbing resolution to the resulting ones delivered by received frame
                    mRecorderCodecContext->width = mSourceResY;
                    mRecorderCodecContext->height = mSourceResY;

                    // allocate software scaler context
                    if (mCodecContext != NULL)
                        mRecorderVideoScalerContext = sws_getContext(mSourceResX, mSourceResY, mCodecContext->pix_fmt, mSourceResX, mSourceResY, mRecorderCodecContext->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
                    else
                    {
                        LOG(LOG_WARN, "Codec context is invalid, pixel format cannot be determined automatically, assuming RGB32 as input");
                        mRecorderVideoScalerContext = sws_getContext(mSourceResX, mSourceResY, PIX_FMT_RGB32, mSourceResX, mSourceResY, mRecorderCodecContext->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
                    }

                    LOG(LOG_INFO, "Resolution changed to (%d * %d)", mSourceResX, mSourceResY);
                }

                // #########################################
                // scale resolution and transform pixel format
                // #########################################
                HM_sws_scale(mRecorderVideoScalerContext, pSourceFrame->data, pSourceFrame->linesize, 0, mSourceResY, mRecorderFinalFrame->data, mRecorderFinalFrame->linesize);
                mRecorderFinalFrame->pict_type = pSourceFrame->pict_type;
                mRecorderFinalFrame->pts = pSourceFrame->pts;
                mRecorderFinalFrame->key_frame = pSourceFrame->key_frame;

                #ifdef MS_DEBUG_RECORDER_FRAMES
                    LOG(LOG_VERBOSE, "Recording video frame..");
                    LOG(LOG_VERBOSE, "      ..key frame: %d", mRecorderFinalFrame->key_frame);
                    LOG(LOG_VERBOSE, "      ..picture type: %s-frame", GetFrameType(mRecorderFinalFrame).c_str());
                    LOG(LOG_VERBOSE, "      ..pts: %"PRId64"", mRecorderFinalFrame->pts);
                    LOG(LOG_VERBOSE, "      ..coded pic number: %d", mRecorderFinalFrame->coded_picture_number);
                    LOG(LOG_VERBOSE, "      ..display pic number: %d", mRecorderFinalFrame->display_picture_number);
                #endif

                if (EncodeAndWritePacket(mRecorderFormatContext, mRecorderCodecContext, mRecorderFinalFrame, tKeyFrame, tBufferedFrames))
                {
                    // increase the frame counter (used for PTS generation)
                    mRecorderFrameNumber++;
                }

                break;

        case MEDIA_AUDIO:
                {
                    int tOutputAudioBytesPerSample = av_get_bytes_per_sample(mRecorderAudioFormat);
                    int tOutputSamplesPerChannel = mRecorderCodecContext->frame_size; // nr. of samples of output frames
                    int tReadFifoSize = tOutputSamplesPerChannel * tOutputAudioBytesPerSample * mRecorderAudioChannels;
                    int tReadFifoSizePerChannel = tOutputSamplesPerChannel * tOutputAudioBytesPerSample;

                    int tInputAudioBytesPerSample = av_get_bytes_per_sample(mInputAudioFormat);
                    int tInputSamplesPerChannel = pSourceFrame->nb_samples; // nr. of samples of source frames

                    int tResamplingOutputSamples = 0;

					#ifdef MS_DEBUG_RECORDER_PACKETS
						if (mCodecContext != NULL)
						{
							if (pSourceFrame->nb_samples != mCodecContext->frame_size)
								LOG(LOG_WARN, "Number of samples in source frame (%d) differs from the defined frame size (%d) in the codec context", pSourceFrame->nb_samples, mCodecContext->frame_size);
						}
                        LOG(LOG_VERBOSE, "Recorder audio input data planes...");
                        for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
                        {
                            LOG(LOG_VERBOSE, "%d - %p", i, pSourceFrame->data[i]);
                        }
                    #endif

                    uint8_t ** tInputSamplesPlanes = pSourceFrame->extended_data;
                    if (mRecorderAudioResampleContext != NULL)
                    {// audio resampling needed: we have to insert an intermediate step, which resamples the audio chunk
                        // ############################
                        // ### resample input
                        // ############################
                        #ifdef MS_DEBUG_RECORDER_PACKETS
                            LOG(LOG_VERBOSE, "Converting %d samples/channel from %p and store it to %p", tInputSamplesPerChannel, *tInputSamplesPlanes, mRecorderResampleBuffer);
                        #endif
                        //LOG(LOG_VERBOSE, "planes are %p and %p", mRecorderResampleBufferPlanes[0], mRecorderResampleBufferPlanes[1]);
                        tResamplingOutputSamples = HM_swr_convert(mRecorderAudioResampleContext, &mRecorderResampleBufferPlanes[0], MEDIA_SOURCE_SAMPLE_BUFFER_PER_CHANNEL, (const uint8_t**)tInputSamplesPlanes, tInputSamplesPerChannel);
                        if (tResamplingOutputSamples <= 0)
                            LOG(LOG_ERROR, "Amount of resampled samples (%d) is invalid", tResamplingOutputSamples);

                        tInputSamplesPlanes = mRecorderResampleBufferPlanes;
                    }

                    int tWrittenFifoSize = tResamplingOutputSamples * tOutputAudioBytesPerSample * mRecorderAudioChannels;
                    int tWrittenFifoSizePerChannel = tResamplingOutputSamples * tOutputAudioBytesPerSample;

                    // ####################################################################
                    // ### buffer the input (resampled?) audio date for frame size conversion
                    // ###################################################################
                    for (int i = 0; i < mRecorderAudioChannels; i++)
                    {
                        int tFifoIndex = i;
                        void *tInputBuffer = (void*)tInputSamplesPlanes[i];
                        if (!av_sample_fmt_is_planar(mRecorderAudioFormat))
                        {
                            tInputBuffer = (void*)(tInputSamplesPlanes[0] + i * tWrittenFifoSizePerChannel);
                            tFifoIndex = 0;
                        }

                        // ############################
                        // ### reallocate FIFO space
                        // ############################
                        #ifdef MS_DEBUG_RECORDER_PACKETS
                            LOG(LOG_VERBOSE, "Adding %d bytes (%d bytes/sample, input channels: %d, frame size: %d) to AUDIO FIFO %d with size of %d bytes", tWrittenFifoSizePerChannel, tInputAudioBytesPerSample, mInputAudioChannels, pSourceFrame->nb_samples, tFifoIndex, av_fifo_size(mRecorderResampleFifo[tFifoIndex]));
                        #endif
                        // is there enough space in the FIFO?
                        if (av_fifo_space(mRecorderResampleFifo[tFifoIndex]) < tWrittenFifoSizePerChannel)
                        {// no, we need reallocation
                            if (av_fifo_realloc2(mRecorderResampleFifo[tFifoIndex], av_fifo_size(mRecorderResampleFifo[tFifoIndex]) + tWrittenFifoSizePerChannel - av_fifo_space(mRecorderResampleFifo[tFifoIndex])) < 0)
                            {
                                // acknowledge failed
                                LOG(LOG_ERROR, "Reallocation of recorder FIFO audio buffer for channel %d failed", i);
                            }
                        }
                        // ############################
                        // ### write frame content to FIFO
                        // ############################
                        #ifdef MS_DEBUG_RECORDER_PACKETS
                            LOG(LOG_VERBOSE, "Writing %d bytes from %u(%u) to FIFO %d", tWrittenFifoSizePerChannel, tInputBuffer, mRecorderResampleBufferPlanes[tFifoIndex], tFifoIndex);
                        #endif
                        av_fifo_generic_write(mRecorderResampleFifo[tFifoIndex], tInputBuffer, tWrittenFifoSizePerChannel, NULL);
                    }

                    // ############################
                    // ### check FIFO for available frames
                    // ############################
                    while (/* planar */((av_sample_fmt_is_planar(mRecorderAudioFormat) &&
                                            (av_fifo_size(mRecorderResampleFifo[0]) >= tReadFifoSizePerChannel))) ||
                          (/* packed/interleaved */((!av_sample_fmt_is_planar(mRecorderAudioFormat)) &&
                                            (av_fifo_size(mRecorderResampleFifo[0]) >= tReadFifoSizePerChannel * mRecorderAudioChannels /* FIFO 0 stores all samples */))))
                    {
                        //####################################################################
                        // create audio planes
                        // ###################################################################
                        uint8_t* tOutputBuffer = (uint8_t*)mRecorderResampleBuffer;
                        for (int i = 0; i < mRecorderAudioChannels; i++)
                        {
                            int tFifoIndex = i;
                            if (!av_sample_fmt_is_planar(mRecorderAudioFormat))
                                tFifoIndex = 0;

                            // ############################
                            // ### read buffer from FIFO
                            // ############################
                            #ifdef MS_DEBUG_RECORDER_PACKETS
                                LOG(LOG_VERBOSE, "Reading %d bytes (%d bytes/sample, frame size: %d samples per packet) from %d bytes of FIFO %d", tReadFifoSizePerChannel, tInputAudioBytesPerSample, mRecorderCodecContext->frame_size, av_fifo_size(mRecorderResampleFifo[tFifoIndex]), tFifoIndex);
                            #endif
                            HM_av_fifo_generic_read(mRecorderResampleFifo[tFifoIndex], (void*)tOutputBuffer, tReadFifoSizePerChannel);

                            tOutputBuffer += tReadFifoSizePerChannel;
                        }

                        //####################################################################
                        // create final frame for audio encoder
                        // ###################################################################
                        avcodec_get_frame_defaults(mRecorderFinalFrame);
                        // nb_samples
                        mRecorderFinalFrame->nb_samples = tOutputSamplesPerChannel;
                        // pts
                        int64_t tCurPts = av_rescale_q(mRecorderFrameNumber * mRecorderCodecContext->frame_size, (AVRational){1, mRecorderAudioSampleRate}, mRecorderCodecContext->time_base);
                        mRecorderFinalFrame->pts = tCurPts;
                        //data
                        int tRes = 0;
                        if ((tRes = avcodec_fill_audio_frame(mRecorderFinalFrame, mRecorderAudioChannels, mRecorderAudioFormat, (const uint8_t *)mRecorderResampleBuffer, tReadFifoSize, 1)) < 0)
                            LOG(LOG_ERROR, "Could not fill the audio frame with the provided data from the audio resampling step because \"%s\"(%d)", strerror(AVUNERROR(tRes)), tRes);
                        #ifdef MS_DEBUG_RECORDER_PACKETS
                            LOG(LOG_VERBOSE, "Recording sample buffer with PTS: %"PRId64" (chunk: %"PRId64"", tCurPts, mRecorderFrameNumber);
                            LOG(LOG_VERBOSE, "Filling audio frame with buffer size: %d", tReadFifoSize);
                        #endif

                        if (EncodeAndWritePacket(mRecorderFormatContext, mRecorderCodecContext, mRecorderFinalFrame, tKeyFrame, tBufferedFrames))
                        {
                            // increase the frame counter (used for PTS generation)
                            mRecorderFrameNumber++;
                        }
                    }
                }

                break;

        default:
                LOG(LOG_ERROR, "Unknown format: %d", mMediaType);
                break;
    }
}

void MediaSource::RecordSamples(int16_t *pSourceSamples, int pSourceSamplesSize)
{
    int     tRes;
    AVFrame *tAudioFrame = AllocFrame();
    if (tAudioFrame == NULL)
    {
        LOG(LOG_ERROR, "Out of memory");
        return;
    }

    //####################################################################
    // create final frame for audio encoder
    // ###################################################################
    #ifdef MS_DEBUG_RECORDER_PACKETS
        LOG(LOG_VERBOSE, "Recording sample buffer of %d bytes", pSourceSamplesSize);
    #endif
    tAudioFrame->nb_samples = pSourceSamplesSize / (mOutputAudioChannels * av_get_bytes_per_sample(mOutputAudioFormat));
    if ((tRes = avcodec_fill_audio_frame(tAudioFrame, mOutputAudioChannels, mOutputAudioFormat, (const uint8_t *)pSourceSamples, pSourceSamplesSize, 1)) < 0)
        LOG(LOG_ERROR, "Could not fill the audio frame with the provided data because \"%s\"(%d)", strerror(AVUNERROR(tRes)), tRes);
    else
        RecordFrame(tAudioFrame);

    av_free(tAudioFrame);
}

bool MediaSource::IsKeyFrame(AVFrame *pFrame)
{
    bool tResult = false;

    if (mMediaType == MEDIA_VIDEO)
    {// video
        switch(pFrame->pict_type)
        {
                case AV_PICTURE_TYPE_NONE:
                    tResult = false;
                    break;
                case AV_PICTURE_TYPE_I:
                    tResult = true;
                    break;
                case AV_PICTURE_TYPE_P:
                    tResult = true;
                    break;
                case AV_PICTURE_TYPE_B:
                    tResult = false;
                    break;
                default:
                    tResult = false;
                    break;
        }
    }else if (mMediaType == MEDIA_AUDIO)
    {// audio
        tResult = true;
    }

    return tResult;
}

string MediaSource::GetFrameType(AVFrame *pFrame)
{
    string tResult = "unknown";

    if (mMediaType == MEDIA_VIDEO)
    {// video
        switch(pFrame->pict_type)
        {
                case AV_PICTURE_TYPE_NONE:
                    tResult = "undef.";
                    break;
                case AV_PICTURE_TYPE_I:
                    tResult = "i";
                    break;
                case AV_PICTURE_TYPE_P:
                    tResult = "p";
                    break;
                case AV_PICTURE_TYPE_B:
                    tResult = "b";
                    break;
                case AV_PICTURE_TYPE_S:
                    tResult = "s";
                    break;
                case AV_PICTURE_TYPE_SI:
                    tResult = "si";
                    break;
                case AV_PICTURE_TYPE_SP:
                    tResult = "sp";
                    break;
                case AV_PICTURE_TYPE_BI:
                    tResult = "bi";
                    break;
                default:
                    tResult = "type " +toString(pFrame->pict_type);
                    break;
        }
    }else if (mMediaType == MEDIA_AUDIO)
    {// audio
        tResult = "audio";
    }
    return tResult;
}

void MediaSource::AnnounceFrame(AVFrame *pFrame)
{
    if (mMediaType == MEDIA_VIDEO)
    {// video
        switch(pFrame->pict_type)
        {
                case AV_PICTURE_TYPE_I:
                    mDecodedIFrames++;
                    break;
                case AV_PICTURE_TYPE_P:
                    mDecodedPFrames++;
                    break;
                case AV_PICTURE_TYPE_B:
                    mDecodedBFrames++;
                    break;
                case AV_PICTURE_TYPE_S: // S(GMC)-VOP MPEG4
                	mDecodedSFrames++;
                	break;
                case AV_PICTURE_TYPE_SI: // Switching Intra
                	mDecodedSIFrames++;
                	break;
                case AV_PICTURE_TYPE_SP: // Switching Predicted
                	mDecodedSPFrames++;
                	break;
                case AV_PICTURE_TYPE_BI: // BI type
                	mDecodedBIFrames++;
                	break;
                default:
                    LOG(LOG_WARN, "Unknown picture type: %d", pFrame->pict_type);
                    break;
        }
    }else if (mMediaType == MEDIA_AUDIO)
    {// audio
        mDecodedIFrames++;
    }
}

string MediaSource::GetSourceTypeStr()
{
    switch (mSourceType)
    {
        case SOURCE_ABSTRACT:
            return "ABSTRACT";
        case SOURCE_MUXER:
            return "MUXER";
        case SOURCE_DEVICE:
            return "DEVICE";
        case SOURCE_MEMORY:
            return "MEMORY";
        case SOURCE_NETWORK:
            return "NETWORK";
        case SOURCE_FILE:
            return "FILE";
        case SOURCE_UNKNOWN:
        default:
            return "unknown";
    }
    return "unknown";
}

enum SourceType MediaSource::GetSourceType()
{
    return mSourceType;
}

string MediaSource::GetMediaTypeStr()
{
    switch (mMediaType)
    {
        case MEDIA_VIDEO:
            return "VIDEO";
        case MEDIA_AUDIO:
            return "AUDIO";
        default:
        	return "VIDEO/AUDIO";
    }
    return "unknown";
}

enum MediaType MediaSource::GetMediaType()
{
	return mMediaType;
}

void MediaSource::getVideoDevices(VideoDevices &pVList)
{
    VideoDeviceDescriptor tDevice;

    tDevice.Name = "NULL: unsupported";
    tDevice.Card = "";
    tDevice.Desc = "Unsupported media type";

    pVList.push_back(tDevice);
}

void MediaSource::getAudioDevices(AudioDevices &pAList)
{
    AudioDeviceDescriptor tDevice;

    tDevice.Name = "NULL: unsupported";
    tDevice.Card = "";
    tDevice.Desc = "Unsupported media type";
    tDevice.IoType = "Input/Output";

    pAList.push_back(tDevice);
}

bool MediaSource::SelectDevice(std::string pDeviceName, enum MediaType pMediaType, bool &pIsNewDevice)
{
    VideoDevices::iterator tVIt;
    VideoDevices tVList;
    AudioDevices::iterator tAIt;
    AudioDevices tAList;
    string tOldDesiredDevice = mDesiredDevice;
    bool tAutoSelect = false;
    int tMediaType = mMediaType;
    bool tResult = false;

    pIsNewDevice = false;

    if ((pMediaType == MEDIA_VIDEO) || (pMediaType == MEDIA_AUDIO))
        tMediaType = pMediaType;

    LOG(LOG_INFO, "%s-Selecting new device..", GetStreamName().c_str());

    if ((pDeviceName == "auto") || (pDeviceName == ""))
    {
        tAutoSelect = true;
        LOG(LOG_VERBOSE, "%s-Automatic device selection..", GetStreamName().c_str());
    }

    switch(tMediaType)
    {
        case MEDIA_VIDEO:
                getVideoDevices(tVList);
                for (tVIt = tVList.begin(); tVIt != tVList.end(); tVIt++)
                {
                    if ((pDeviceName == tVIt->Name) || (tAutoSelect))
                    {
                        mDesiredDevice = tVIt->Card;
                        mCurrentDeviceName = tVIt->Name;
                        if (mDesiredDevice != mCurrentDevice)
                        {
                            pIsNewDevice = true;
                            LOG(LOG_INFO, "%s-Video device selected: %s", GetStreamName().c_str(), mDesiredDevice.c_str());
                        }else
                            LOG(LOG_INFO, "%s-Video device re-selected: %s", GetStreamName().c_str(), mDesiredDevice.c_str());
                        tResult = true;
                        break;
                    }
                }
                break;
        case MEDIA_AUDIO:
                getAudioDevices(tAList);
                for (tAIt = tAList.begin(); tAIt != tAList.end(); tAIt++)
                {
                    if ((pDeviceName == tAIt->Name) || (tAutoSelect))
                    {
                        mDesiredDevice = tAIt->Card;
                        mCurrentDeviceName = tAIt->Name;
                        if (mDesiredDevice != mCurrentDevice)
                        {
                            pIsNewDevice = true;
                            LOG(LOG_INFO, "%s-Audio device selected: %s", GetStreamName().c_str(), mDesiredDevice.c_str());
                        }else
                            LOG(LOG_INFO, "%s-Audio device re-selected: %s", GetStreamName().c_str(), mDesiredDevice.c_str());
                        tResult = true;
                        break;
                    }
                }
                break;
        default:
                LOG(LOG_ERROR, "%s-Media type undefined", GetStreamName().c_str());
                break;
    }

    if ((!pIsNewDevice) && (((pDeviceName == "auto") || (pDeviceName == "automatic") || pDeviceName == "")))
    {
        LOG(LOG_INFO, "%s-Auto detected device was selected: auto detect (card: auto)", GetStreamName().c_str());
        mDesiredDevice = "";
        mCurrentDeviceName = "auto selection";
    }

    LOG(LOG_VERBOSE, "%s-Source should be reset: %d", GetStreamName().c_str(), pIsNewDevice);
    if (!tResult)
        LOG(LOG_INFO, "%s-Selected device %s is not available", GetStreamName().c_str(), pDeviceName.c_str());

    return tResult;
}

std::string MediaSource::GetCurrentDevicePeerName()
{
	return "";
}

std::string MediaSource::GetCurrentDeviceName()
{
	return mCurrentDeviceName;
}

bool MediaSource::RegisterMediaSource(MediaSource* pMediaSource)
{
    LOG(LOG_VERBOSE, "Registering media source: 0x%x", pMediaSource);
    LOG(LOG_VERBOSE, "This is only the dummy RegisterMediaSource() function");
    return true;
}

bool MediaSource::UnregisterMediaSource(MediaSource* pMediaSource, bool pAutoDelete)
{
    LOG(LOG_VERBOSE, "Unregistering media source: 0x%x", pMediaSource);
    LOG(LOG_VERBOSE, "This is only the dummy UnregisterMediaSource() function");
    return true;
}

float MediaSource::GetInputFrameRate()
{
    return mInputFrameRate;
}

float MediaSource::GetOutputFrameRate()
{
    return mOutputFrameRate;
}

void MediaSource::SetFrameRate(float pFps)
{
    if (pFps > 29.97)
        pFps = 29.97;
    if (pFps < 5)
        pFps = 5;
    mInputFrameRate = pFps;
}

int64_t MediaSource::GetSynchronizationTimestamp()
{
    return 0;
}

int MediaSource::GetSynchronizationPoints()
{
    return mDecoderSynchPoints;
}

bool MediaSource::TimeShift(int64_t pOffset)
{
    LOG(LOG_VERBOSE, "This is only the dummy TimeShift() function");
    return false;
}

float MediaSource::GetRelativeLoss()
{
    return mRelativeLoss;
}

void* MediaSource::AllocChunkBuffer(int& pChunkBufferSize, enum MediaType pMediaType)
{
    enum MediaType tMediaType = mMediaType;

    if ((pMediaType == MEDIA_UNKNOWN) && (!mMediaSourceOpened))
    {
        LOG(LOG_ERROR, "Tried to allocate a chunk buffer while media type is undefined");
        return NULL;
    }

    if ((pMediaType == MEDIA_VIDEO) || (pMediaType == MEDIA_AUDIO))
        tMediaType = pMediaType;

    switch(tMediaType)
    {
        case MEDIA_VIDEO:
            pChunkBufferSize = avpicture_get_size(PIX_FMT_RGB32, mTargetResX, mTargetResY) + FF_INPUT_BUFFER_PADDING_SIZE;
            LOG(LOG_VERBOSE, "Allocating %d bytes video buffer for %d*%d RGB32 pictures", pChunkBufferSize, mTargetResX, mTargetResY);
            return av_malloc(pChunkBufferSize);
        case MEDIA_AUDIO:
            pChunkBufferSize = MEDIA_SOURCE_SAMPLES_MULTI_BUFFER_SIZE * 2 + FF_INPUT_BUFFER_PADDING_SIZE;
            return av_malloc(pChunkBufferSize);
        default:
        	LOG(LOG_WARN, "Undefined media type, returning chunk buffer will be invalid");
            return NULL;
    }
}

void MediaSource::FreeChunkBuffer(void *pChunk)
{
    av_free(pChunk);
}

void MediaSource::FreeUnusedRegisteredFileSources()
{
}

bool MediaSource::SupportsSeeking()
{
    return false;
}

float MediaSource::GetSeekEnd()
{
    return 0;
}

bool MediaSource::Seek(float pSeconds, bool pOnlyKeyFrames)
{
    return false;
}

float MediaSource::GetSeekPos()
{
    return 0;
}

bool MediaSource::SupportsMultipleInputStreams()
{
    return false;
}

bool MediaSource::SelectInputStream(int pIndex)
{
    return false;
}

vector<string> MediaSource::GetInputStreams()
{
    vector<string> tResult;

    return tResult;
}

bool MediaSource::HasInputStreamChanged()
{
	return false;
}

string MediaSource::CurrentInputStream()
{
    return mCurrentDevice;
}

bool MediaSource::SupportsMarking()
{
    return false;
}

bool MediaSource::MarkerActive()
{
    return mMarkerActivated;
}

void MediaSource::SetMarker(bool pActivation)
{
    LOG(LOG_VERBOSE, "Setting marker state to %d", pActivation);
    mMarkerActivated = pActivation;
}

void MediaSource::MoveMarker(float pRelX, float pRelY)
{
    if (MarkerActive())
    {
        //LOG(LOG_VERBOSE, "Moving marker to %.2f, %.2f", pRelX, pRelY);
        mMarkerRelX = pRelX;
        mMarkerRelY = pRelY;
    }
}

void MediaSource::InitFpsEmulator()
{
    mSourceStartTimeForRTGrabbing = av_gettime();
}

int64_t MediaSource::GetPtsFromFpsEmulator()
{
    int64_t tRelativeRealTimeUSecs = av_gettime() - mSourceStartTimeForRTGrabbing; // relative playback time in usecs
    float tRelativeFrameNumber = GetInputFrameRate() * tRelativeRealTimeUSecs / AV_TIME_BASE;
    return (int64_t)tRelativeFrameNumber;
}

bool MediaSource::ContainsOnlySilence(void* pChunkBuffer, int pChunkSize)
{
	bool tResult = true;
	short int tSample;

	// scan all samples
	for (int i = 0; i < pChunkSize / 4; i++)
	{
		tSample = *((int16_t*)pChunkBuffer + i * 2);
		if ((tSample > mAudioSilenceThreshold) || (tSample < -mAudioSilenceThreshold))
		{// we detected an interesting sample
			//LOG(LOG_VERBOSE, "%hd %d %d", i, tSample, mAudioSilenceThreshold);
			tResult = false;
			break;
		}
	}

	return tResult;
}

int64_t FilterNeg(int64_t pValue)
{
    if (pValue > 0)
        return pValue;
    else
        return 0;
}

void MediaSource::EventOpenGrabDeviceSuccessful(string pSource, int pLine)
{
    char tChannelLayoutStr[512];

    //######################################################
    //### give some verbose output
    //######################################################
    LOG_REMOTE(LOG_INFO, pSource, pLine, "%s source opened...", GetMediaTypeStr().c_str());
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..codec name: %s", mCodecContext->codec->name);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..codec long name: %s", mCodecContext->codec->long_name);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..codec flags: 0x%x", mCodecContext->flags);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..codec time_base: %d/%d", mCodecContext->time_base.num, mCodecContext->time_base.den); // inverse
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..stream ID: %d", mMediaStreamIndex);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..stream start real-time: %"PRId64"", mFormatContext->start_time_realtime);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..stream start time: %"PRId64"", FilterNeg(mFormatContext->streams[mMediaStreamIndex]->start_time));
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..stream rfps: %d/%d", mFormatContext->streams[mMediaStreamIndex]->r_frame_rate.num, mFormatContext->streams[mMediaStreamIndex]->r_frame_rate.den);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..stream time_base: %d/%d", mFormatContext->streams[mMediaStreamIndex]->time_base.num, mFormatContext->streams[mMediaStreamIndex]->time_base.den); // inverse
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..stream codec time_base: %d/%d", mFormatContext->streams[mMediaStreamIndex]->codec->time_base.num, mFormatContext->streams[mMediaStreamIndex]->codec->time_base.den); // inverse
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..bit rate: %d bit/s", mCodecContext->bit_rate);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..desired device: %s", mDesiredDevice.c_str());
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..current device: %s", mCurrentDevice.c_str());
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..qmin: %d", mCodecContext->qmin);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..qmax: %d", mCodecContext->qmax);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..decoding delay: %d", mCodecContext->delay);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..decoding profile: %d", mCodecContext->profile);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..decoding level: %d", mCodecContext->level);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..codec caps: 0x%x", mCodecContext->codec->capabilities);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..MT count: %d", mCodecContext->thread_count);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..MT method: %d", mCodecContext->thread_type);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..frame size: %d", mCodecContext->frame_size);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..duration: %.2f frames", mNumberOfFrames);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..input start PTS: %"PRId64" frames", FilterNeg(mInputStartPts));
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..start CTX PTS: %"PRId64" frames", FilterNeg(mFormatContext->start_time));
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..start CTX PTS (RT): %"PRId64" frames", FilterNeg(mFormatContext->start_time_realtime));
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..format context duration: %"PRId64" seconds (exact value: %"PRId64")", FilterNeg(mFormatContext->duration) / AV_TIME_BASE, mFormatContext->duration);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..input frame rate: %.2f fps", GetInputFrameRate());
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..output frame rate: %.2f fps", GetOutputFrameRate());
    int64_t tStreamDuration = FilterNeg(mFormatContext->streams[mMediaStreamIndex]->duration);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..stream context duration: %"PRId64" frames (%.0f seconds), nr. of frames: %"PRId64"", tStreamDuration, (float)tStreamDuration / GetInputFrameRate());
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..stream context frames: %"PRId64"", mFormatContext->streams[mMediaStreamIndex]->nb_frames);
    LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..max. delay: %d", mFormatContext->max_delay);
    switch(mMediaType)
    {
        case MEDIA_VIDEO:
            LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..source resolution: %d * %d", mCodecContext->width, mCodecContext->height);
            LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..target resolution: %d * %d", mTargetResX, mTargetResY);
            LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..i-frame distance: %d", mCodecContext->gop_size);
            LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..mpeg quant: %d", mCodecContext->mpeg_quant);
            LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..pixel format: %d", (int)mCodecContext->pix_fmt);
            break;
        case MEDIA_AUDIO:
            av_get_channel_layout_string(tChannelLayoutStr, 512, mCodecContext->channels, mCodecContext->channel_layout);
            LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..sample rate: %d", mCodecContext->sample_rate);
            LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..channels: %d [layout: %s]", mCodecContext->channels, tChannelLayoutStr);
            LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..sample format: %s", av_get_sample_fmt_name(mCodecContext->sample_fmt));
            LOG_REMOTE(LOG_INFO, pSource, pLine, "    ..audio preload: %d", mFormatContext->audio_preload);
            break;
        default:
            LOG(LOG_ERROR, "Media type unknown");
            break;
    }

    //######################################################
    //### initiate local variables
    //######################################################
    InitFpsEmulator();
    mFrameNumber = 0;
    mDecoderFrameBufferTime = 0;
    mChunkDropCounter = 0;
    mLastGrabFailureReason = "";
    mLastGrabResultWasError = false;
    mMediaSourceOpened = true;
    mEOFReached = false;
    mDecodedIFrames = 0;
    mDecodedPFrames = 0;
    mDecodedBFrames = 0;
	mDecodedSFrames = 0;
	mDecodedSIFrames = 0;
	mDecodedSPFrames = 0;
	mDecodedBIFrames = 0;
}

void MediaSource::EventGrabChunkSuccessful(string pSource, int pLine, int pChunkNumber)
{
    if (mLastGrabResultWasError)
    {
        switch(mMediaType)
        {
            case MEDIA_VIDEO:
                LOG_REMOTE(LOG_WARN, pSource, pLine, "Grabbing successful with chunk %d -recovered from error state of last video grabbing", pChunkNumber);
                break;
            case MEDIA_AUDIO:
                LOG_REMOTE(LOG_WARN, pSource, pLine, "Grabbing successful with chunk %d -recovered from error state of last audio grabbing", pChunkNumber);
                break;
            default:
                LOG_REMOTE(LOG_ERROR, pSource, pLine, "Media type unknown");
                LOG_REMOTE(LOG_WARN, pSource, pLine, "Grabbing successful with chunk %d -recovered from error state of last grabbing", pChunkNumber);
                break;
        }
    }
    mLastGrabResultWasError = false;
}

void MediaSource::EventGrabChunkFailed(string pSource, int pLine, string pReason)
{
    if ((!mLastGrabResultWasError) || (mLastGrabFailureReason != pReason))
    {
        LOG_REMOTE(LOG_ERROR, pSource, pLine, "Grabbing failed because \"%s\"", pReason.c_str());
    }
    mLastGrabResultWasError = true;
    mLastGrabFailureReason = pReason;
}

// ####################################################################################
// ### FFMPEG helpers
// ####################################################################################
bool MediaSource::FfmpegDescribeInput(string pSource, int pLine, AVCodecID pCodecId, AVInputFormat **pFormat)
{
	AVInputFormat *tResult = NULL;

    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Going to find %s input format for codec %d..", GetMediaTypeStr().c_str(), pCodecId);

	if (pFormat == NULL)
	{
		LOG_REMOTE(LOG_ERROR, pSource, pLine, "Invalid format pointer");

		return false;
	}

    // derive codec name from codec ID
    string tCodecName = GetFormatName(pCodecId);
    // ffmpeg knows only the mpegvideo demuxer which is responsible for both MPEG1 and MPEG2 streams
    if ((tCodecName == "mpeg1video") || (tCodecName == "mpeg2video"))
        tCodecName = "mpegvideo";

	if ((tResult = av_find_input_format(tCodecName.c_str())) == NULL)
    {
        if (!mGrabbingStopped)
        	LOG_REMOTE(LOG_ERROR, pSource, pLine, "Couldn't find %s input format for codec %s", GetMediaTypeStr().c_str(), tCodecName.c_str());

        return NULL;
    }

	LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Successfully found %s input format with flags: %d", GetMediaTypeStr().c_str(), tResult->flags);

    *pFormat = tResult;

	return true;
}

bool MediaSource::FfmpegCreateIOContext(string pSource, int pLine, char *pPacketBuffer, int pPacketBufferSize, IOFunction pReadFunction, IOFunction pWriteFunction, void *pOpaque, AVIOContext **pIoContext)
{
	AVIOContext *tResult = NULL;

	if (pIoContext == NULL)
	{
		LOG_REMOTE(LOG_ERROR, pSource, pLine, "Invalid I/O context pointer");

		return false;
	}

	// create the I/O context
	tResult = avio_alloc_context((uint8_t*) pPacketBuffer, pPacketBufferSize, (pWriteFunction != NULL) ? 1 : 0 /* read-only? */, pOpaque, pReadFunction, pWriteFunction, NULL);

	//pPacketBuffermStreamPacketBuffer, MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE, /* read-only */0, this, GetNextPacket, NULL, NULL);
	tResult->seekable = 0;

	// limit packet size, otherwise ffmpeg will deliver unpredictable results ;)
	tResult->max_packet_size = pPacketBufferSize;

    *pIoContext = tResult;

	return true;
}

int MediaSource::FindStreamInfoCallback(void *pMediaSource)
{
    MediaSource* tMediaSource = (MediaSource*)pMediaSource;

    //LOGEX(MediaSource, LOG_WARN, "Ffmpeg calls us to ask for interrupt for %s %s source", tMediaSource->GetMediaTypeStr().c_str(), tMediaSource->GetSourceTypeStr().c_str());

    bool tResult = tMediaSource->mGrabbingStopped;
    //LOGEX(MediaSource, LOG_WARN, "Interrupt the running process: %d", tResult);

    return (int)tResult;
}

bool MediaSource::FfmpegOpenInput(string pSource, int pLine, const char *pInputName, AVInputFormat *pInputFormat, AVIOContext *pIOContext)
{
	int 				tRes = 0;

    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Going to open %s input..", GetMediaTypeStr().c_str());

    if (mMediaSourceOpened)
    {
    	LOG_REMOTE(LOG_ERROR, pSource, pLine, "%s source already open", GetMediaTypeStr().c_str());

        return false;
    }

    // alocate new format context
    mFormatContext = AV_NEW_FORMAT_CONTEXT(); // make sure we have default values in format context, otherwise avformat_open_input() will crash
    if (mFormatContext == NULL)
        LOG_REMOTE(LOG_ERROR, pSource, pLine, "Couldn't allocate and initialize format context");

    // define IO context if there is a customized one
    mFormatContext->pb = pIOContext;

    // open input: automatic content detection is done inside ffmpeg
    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "    ..calling avformat_open_input() for %s source", GetMediaTypeStr().c_str());
    mFormatContext->interrupt_callback.callback = FindStreamInfoCallback;
    mFormatContext->interrupt_callback.opaque = this;
	if ((tRes = avformat_open_input(&mFormatContext, pInputName, pInputFormat, NULL)) < 0)
	{
    	LOG_REMOTE(LOG_ERROR, pSource, pLine, "Couldn't open %s input \"%s\" because \"%s\"(%d)", GetMediaTypeStr().c_str(), pInputName, strerror(AVUNERROR(tRes)), tRes);

		return false;
	}

    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Setting %s device (name) to %s", GetMediaTypeStr().c_str(), pInputName);
    mCurrentDevice = pInputName;
    mCurrentDeviceName = pInputName;

    if (mGrabbingStopped)
    {
        LOG_REMOTE(LOG_WARN, pSource, pLine, "%s-Grabbing was stopped during avformat_open_input(), returning immediately", GetMediaTypeStr().c_str());
        return false;
    }

    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "%s input opened", GetMediaTypeStr().c_str());

	return true;
}

bool MediaSource::FfmpegDetectAllStreams(string pSource, int pLine)
{
	int 				tRes = 0;

    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Going to detect all existing streams in input for selecting a %s stream later..", GetMediaTypeStr().c_str());

    // limit frame analyzing time for ffmpeg internal codec auto detection for non file based media sources
    if (GetSourceType() != SOURCE_FILE)
    {
        if (mMediaType == MEDIA_AUDIO)
        {// we limit the analyzing time to a quarter
            mFormatContext->max_analyze_duration = AV_TIME_BASE / 4;
        }else{
        	switch(mSourceCodecId)
        	{
                case AV_CODEC_ID_MPEG1VIDEO:
                case AV_CODEC_ID_MPEG2VIDEO:
                case AV_CODEC_ID_MPEG4:
                case AV_CODEC_ID_H264:
                    // we shouldn't limit the analyzing time because the analyzer needs the entire time period to deliver a reliable result
                    break;
                default:
                    // we may limit the analyzing time to the half
                    mFormatContext->max_analyze_duration = AV_TIME_BASE / 2;
                    break;
        	}
        }
    }

    // verbose timestamp debugging
    if (LOGGER.GetLogLevel() == LOG_WORLD)
    {
        LOG_REMOTE(LOG_WARN, pSource, pLine, "Enabling ffmpeg timestamp debugging for %s decoder", GetMediaTypeStr().c_str());
        mFormatContext->debug = FF_FDEBUG_TS;
    }

    // discard all corrupted frames
    mFormatContext->flags |= AVFMT_FLAG_DISCARD_CORRUPT;

    //LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Current format context flags: %d, packet buffer: %p, raw packet buffer: %p, nb streams: %d", mFormatContext->flags, mFormatContext->packet_buffer, mFormatContext->raw_packet_buffer, mFormatContext->nb_streams);
    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "    ..calling avformat_find_stream_info() for %s source", GetMediaTypeStr().c_str());
    if ((tRes = avformat_find_stream_info(mFormatContext, NULL)) < 0)
    {
        if (!mGrabbingStopped)
        	LOG_REMOTE(LOG_ERROR, pSource, pLine, "Couldn't find %s stream information because \"%s\"(%d)", GetMediaTypeStr().c_str(), strerror(AVUNERROR(tRes)), tRes);
        else
            LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Grabbing was stopped during avformat_find_stream_info()");

        // Close the video stream
        HM_avformat_close_input(mFormatContext);

        return false;
    }

	LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Detected all %s streams", GetMediaTypeStr().c_str());

    return true;
}

bool MediaSource::FfmpegSelectStream(string pSource, int pLine)
{
    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Going to find fitting %s stream..", GetMediaTypeStr().c_str());

    // reset used media stream ID
    mMediaStreamIndex = -1;

	enum AVMediaType tTargetMediaType;
	string tMediaSource = "";
	string tTargetMediaDescription = "unknown";
	int tPos = pSource.rfind(':');
	if (tPos != (int)string::npos)
		tMediaSource = pSource.substr(tPos + 1, pSource.length() - tPos- 1);

	switch(mMediaType)
	{
		case MEDIA_VIDEO:
			tTargetMediaType = AVMEDIA_TYPE_VIDEO;
			tTargetMediaDescription = tMediaSource + "(video)";
			break;
		case MEDIA_AUDIO:
			tTargetMediaType = AVMEDIA_TYPE_AUDIO;
			tTargetMediaDescription = tMediaSource + "(audio)";
			break;
		default:
			break;
	}

    //######################################################
    //### check all detected streams for a matching one
    //######################################################
	for (int i = 0; i < (int)mFormatContext->nb_streams; i++)
	{
	    //######################################################
	    //### dump ffmpeg information about format
	    //######################################################
	    if(mFormatContext->streams[i]->codec->codec_type == tTargetMediaType)
	    {
	        av_dump_format(mFormatContext, i, tTargetMediaDescription.c_str(), false);
	        mMediaStreamIndex = i;
	        break;
	    }
	}

	if (mMediaStreamIndex == -1)
    {
	    LOG_REMOTE(LOG_ERROR, pSource, pLine, "Couldn't find a %s stream..", GetMediaTypeStr().c_str());

	    // Close the video stream
	    HM_avformat_close_input(mFormatContext);

        return false;
    }

	LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Found %s stream at index %d", GetMediaTypeStr().c_str(), mMediaStreamIndex);

	return true;
}

bool MediaSource::FfmpegOpenDecoder(string pSource, int pLine)
{
	int 				tRes = 0;
    AVCodec             *tCodec = NULL;
    AVDictionary        *tOptions = NULL;

    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Going to open %s decoder..", GetMediaTypeStr().c_str());

    // Get a pointer to the codec context for the video stream
    mCodecContext = mFormatContext->streams[mMediaStreamIndex]->codec;

    // check for VDPAU support
    if (mCodecContext->codec)
	{
    	if (mCodecContext->codec->capabilities & CODEC_CAP_HWACCEL_VDPAU)
        	LOG_REMOTE(LOG_WARN, pSource, pLine, "%s codec %s supports HW decoding (VDPAU)!", GetMediaTypeStr().c_str(), mCodecContext->codec->name);

    	if (mCodecContext->codec->capabilities & CODEC_CAP_HWACCEL)
        	LOG_REMOTE(LOG_WARN, pSource, pLine, "%s codec %s supports HW decoding (XvMC)!", GetMediaTypeStr().c_str(), mCodecContext->codec->name);

    	if (mCodecContext->codec->capabilities & CODEC_CAP_SUBFRAMES)
        	LOG_REMOTE(LOG_WARN, pSource, pLine, "%s codec %s supports SUB FRAMES!", GetMediaTypeStr().c_str(), mCodecContext->codec->name);

        if (mCodecContext->codec->capabilities & CODEC_CAP_DELAY)
            LOG_REMOTE(LOG_WARN, pSource, pLine, "%s codec %s supports might DELAY FRAMES!", GetMediaTypeStr().c_str(), mCodecContext->codec->name);

        if (mCodecContext->codec->capabilities & CODEC_CAP_TRUNCATED)
            LOG_REMOTE(LOG_WARN, pSource, pLine, "%s codec %s supports TRUNCATED PACKETS!", GetMediaTypeStr().c_str(), mCodecContext->codec->name);
	}

    // make sure we have a defined frame size
    if (mCodecContext->frame_size < 32)
    {
        LOG_REMOTE(LOG_WARN, pSource, pLine, "Found invalid frame size %d, setting to %d", mCodecContext->frame_size, MEDIA_SOURCE_SAMPLES_PER_BUFFER);
        mCodecContext->frame_size = MEDIA_SOURCE_SAMPLES_PER_BUFFER;
    }

    switch(mMediaType)
    {
		case MEDIA_VIDEO:
			// set grabbing resolution and frame-rate to the resulting ones delivered by opened video codec
			mSourceResX = mCodecContext->width;
			mSourceResY = mCodecContext->height;

			// fall back method for resolution detection
		    if ((mSourceResX == 0) && (mSourceResY == 0))
		    {
		        mSourceResX = mCodecContext->coded_width;
		        mSourceResY = mCodecContext->coded_height;
		    }

		    mOutputFrameRate = (float)mFormatContext->streams[mMediaStreamIndex]->r_frame_rate.num / mFormatContext->streams[mMediaStreamIndex]->r_frame_rate.den;

		    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Detected video resolution: %d*%d", mSourceResX, mSourceResY);
		    break;

		case MEDIA_AUDIO:

			// set sample rate and bit rate to the resulting ones delivered by opened audio codec
			mInputAudioSampleRate = mCodecContext->sample_rate;
			mInputAudioChannels = mCodecContext->channels;
			mInputAudioFormat = mCodecContext->sample_fmt;
			mOutputFrameRate = (float)mOutputAudioSampleRate /* 44100 samples per second */ / MEDIA_SOURCE_SAMPLES_PER_BUFFER /* 1024 samples per frame */;

		    break;

		default:
			break;
    }

	mInputBitRate = mFormatContext->streams[mMediaStreamIndex]->codec->bit_rate;

	// derive the FPS from the timebase of the selected input stream
	mInputFrameRate = (float)mFormatContext->streams[mMediaStreamIndex]->time_base.den / mFormatContext->streams[mMediaStreamIndex]->time_base.num;

    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Detected frame rate: %f", GetInputFrameRate());

    if ((mMediaType == MEDIA_VIDEO) && ((mSourceResX == 0) || (mSourceResY == 0)))
    {
    	LOG_REMOTE(LOG_ERROR, pSource, pLine, "Couldn't detect VIDEO resolution information within input stream");

        // Close the video file
    	HM_avformat_close_input(mFormatContext);

        return false;
    }

    //######################################################
    //### search for correct decoder for the A/V stream
    //######################################################
    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "..going to find %s decoder..", GetMediaTypeStr().c_str());

    #ifdef MEDIA_SOURCE_VDPAU_SUPPORT
        // try to find a vdpau decoder
        switch(mCodecContext->codec_id)
        {
            case AV_CODEC_ID_H264:
                tCodec = avcodec_find_decoder_by_name("h264_vdpau");
                break;
            case AV_CODEC_ID_MPEG1VIDEO:
                tCodec = avcodec_find_decoder_by_name("mpeg1video_vdpau");
                break;
            case AV_CODEC_ID_MPEG2VIDEO:
                tCodec = avcodec_find_decoder_by_name("mpegvideo_vdpau");
                break;
            case AV_CODEC_ID_MPEG4:
                tCodec = avcodec_find_decoder_by_name("mpeg4_vdpau");
                break;
            case AV_CODEC_ID_WMV3:
                tCodec = avcodec_find_decoder_by_name("wmv3_vdpau");
                break;
            default:
                break;
        }
    #endif
    //TODO: additional VDPAU stuff needed here

    // try to find a standard decoder
    if (tCodec == NULL)
        tCodec = avcodec_find_decoder(mCodecContext->codec_id);

    if(tCodec == NULL)
    {
    	LOG_REMOTE(LOG_ERROR, pSource, pLine, "Couldn't find a fitting %s codec", GetMediaTypeStr().c_str());

        // Close the video stream
    	HM_avformat_close_input(mFormatContext);

        return false;
    }

    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "..successfully found %s decoder", GetMediaTypeStr().c_str());

    //H.264: force thread count to 1 since the h264 decoder will not extract SPS and PPS to extradata during multi-threaded decoding
    if (mCodecContext->codec_id == AV_CODEC_ID_H264)
    {
            if (strcmp(mFormatContext->filename, "") == 0)
            {// we have a net/mem based media source
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Disabling MT during avcodec_open2() for H264 codec");

                // disable MT for H264, otherwise the decoder runs into trouble
                av_dict_set(&tOptions, "threads", "1", 0);

                mCodecContext->thread_count = 1;
            }else
            {// we have a file based media source and we can try to decode in MT mode
                LOG_REMOTE(LOG_WARN, pSource, pLine, "Trying to decode H.264 with MT support");
            }
    }

    if (tCodec->capabilities & CODEC_CAP_DR1)
    	mCodecContext->flags |= CODEC_FLAG_EMU_EDGE;

    //######################################################
    //### open the selected codec
    //######################################################
    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "..going to open %s codec..", GetMediaTypeStr().c_str());
    if ((tRes = HM_avcodec_open(mCodecContext, tCodec, &tOptions)) < 0)
    {
    	LOG_REMOTE(LOG_ERROR, pSource, pLine, "Couldn't open video codec because \"%s\"(%d)", strerror(AVUNERROR(tRes)), tRes);

    	HM_avformat_close_input(mFormatContext);

    	return false;
    }

    if (tCodec->capabilities & CODEC_CAP_DELAY)
    {
        LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "%s decoder output might be delayed for %s codec", GetMediaTypeStr().c_str(), mCodecContext->codec->name);
    }

    //HINT: we allow the input bit stream to be truncated at packet boundaries instead of frame boundaries,
    //		otherwise an UDP/TCP based transmission will fail because the decoder expects only complete packets as input
    mCodecContext->flags2 |= CODEC_FLAG2_CHUNKS | CODEC_FLAG2_SHOW_ALL;

    //set duration
    if (mFormatContext->duration > 0)
    {
        mNumberOfFrames = GetInputFrameRate() * mFormatContext->duration / AV_TIME_BASE;
        LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Number of frames set to: %.2f, fps: %.2f, format context duration: %"PRId64"", (float)mNumberOfFrames, GetInputFrameRate(), mFormatContext->duration);
    }else
    {
        LOG_REMOTE(LOG_WARN, pSource, pLine, "Found duration of %s stream is invalid, will use a value of 0 instead", GetMediaTypeStr().c_str());
    	mNumberOfFrames = 0;
    }

    // set PTS offset
    if (mFormatContext->start_time > 0)
    {
    	mInputStartPts = GetInputFrameRate() * mFormatContext->start_time / AV_TIME_BASE;
    	LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Setting %s start time to %"PRId64, GetMediaTypeStr().c_str(), mInputStartPts);
    }else
    {
        LOG_REMOTE(LOG_WARN, pSource, pLine, "Found start time of %s stream is invalid, will use a value of 0 instead", GetMediaTypeStr().c_str());
    	mInputStartPts = 0;
    }

    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "..successfully opened %s decoder", GetMediaTypeStr().c_str());

    return true;
}


bool MediaSource::FfmpegOpenFormatConverter(string pSource, int pLine)
{
    int tRes;

    switch (mMediaType)
	{
		case MEDIA_VIDEO:
		    // create context for picture scaler
		    mVideoScalerContext = sws_getContext(mCodecContext->width, mCodecContext->height, mCodecContext->pix_fmt, mTargetResX, mTargetResY, PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);
			break;
		case MEDIA_AUDIO:
			{
                // create resample context
                if (mAudioResampleContext != NULL)
                    LOG_REMOTE(LOG_ERROR, pSource, pLine, "State of audio resample context inconsistent");
                LOG_REMOTE(LOG_WARN, pSource, pLine, "Audio samples with rate of %d Hz and %d channels (format: %s) have to be resampled to %d Hz and %d channels (format: %s)", mInputAudioSampleRate, mInputAudioChannels, av_get_sample_fmt_name(mInputAudioFormat), mOutputAudioSampleRate, mOutputAudioChannels, av_get_sample_fmt_name(mOutputAudioFormat));
                mAudioResampleContext = HM_swr_alloc_set_opts(NULL, av_get_default_channel_layout(mOutputAudioChannels), mOutputAudioFormat, mOutputAudioSampleRate, av_get_default_channel_layout(mInputAudioChannels), mInputAudioFormat, mInputAudioSampleRate, 0, NULL);
                if (mAudioResampleContext != NULL)
                {// everything okay, we have to init the context
                    int tRes = 0;
                    if ((tRes = HM_swr_init(mAudioResampleContext)) < 0)
                    {
                        LOG_REMOTE(LOG_ERROR, pSource, pLine, "Couldn't initialize resample context because \"%s\"(%d)", strerror(AVUNERROR(tRes)), tRes);
                        return false;
                    }
                }else
                {
                    LOG_REMOTE(LOG_ERROR, pSource, pLine, "Failed to create audio-resample context");
                }

                // resample buffer
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "..allocating audio resample memory");
                if (mResampleBuffer != NULL)
                    LOG_REMOTE(LOG_ERROR, pSource, pLine, "Resample buffer of %s source was already allocated", GetSourceTypeStr().c_str());
                mResampleBuffer = (char*)malloc(MEDIA_SOURCE_SAMPLE_BUFFER_PER_CHANNEL * 32 + FF_INPUT_BUFFER_PADDING_SIZE);

//                LOG(LOG_VERBOSE, "..allocating final frame memory");
//                if ((mFinalFrame = AllocFrame()) == NULL)
//                    LOG(LOG_ERROR, "Out of memory in avcodec_alloc_frame()");

                // reset the structure
                memset((void*)&mResampleFifo[0], 0, sizeof(mResampleFifo));

                if (av_sample_fmt_is_planar(mOutputAudioFormat))
                {// planar audio buffering
                    // init fifo buffer per channel
                    for (int i = 0; i < mOutputAudioChannels; i++)
                    {
                    	LOG(LOG_VERBOSE, "Allocating AUDIO resample FIFO %d", i);
                    	mResampleFifo[i] = HM_av_fifo_alloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
                    }
                }else
                {// one interleaved audio buffer
                	LOG(LOG_VERBOSE, "Allocating AUDIO resample FIFO %d", 0);
                    mResampleFifo[0] = HM_av_fifo_alloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
                }

                // planes indexing resample buffer
                int InputFrameSize = (mCodecContext != NULL) ? (mCodecContext->frame_size > 0 ? mCodecContext->frame_size : MEDIA_SOURCE_SAMPLES_PER_BUFFER) : MEDIA_SOURCE_SAMPLES_PER_BUFFER;
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "..assigning planes memory for indexing resample memory with frame size: %d", InputFrameSize /* we use the source frame size! */);
                memset(mResampleBufferPlanes, 0, sizeof(uint8_t*) * MEDIA_SOURCE_MAX_AUDIO_CHANNELS);
                if (HM_av_samples_fill_arrays(&mResampleBufferPlanes[0], NULL, (uint8_t *)mResampleBuffer, mOutputAudioChannels, InputFrameSize /* we use the source frame size! */, mOutputAudioFormat, 1) < 0)
                {
                    LOG_REMOTE(LOG_ERROR, pSource, pLine, "Could not fill the audio plane pointer array");
                }
                //AVCODEC_MAX_AUDIO_FRAME_SIZE / (av_get_bytes_per_sample(mOutputAudioFormat) * mOutputAudioChannels) /* amount of possible output samples */;
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Resampling buffer is at: %p", mResampleBuffer);
                for(int i = 0; i < mOutputAudioChannels; i++)
                {
                    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Plane %d index points to: %p (diff: %u)", i, mResampleBufferPlanes[i], (i > 0) ? (mResampleBufferPlanes[i] - mResampleBufferPlanes[i - 1]) : 0);
                }
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Output audio format is planar: %d", av_sample_fmt_is_planar(mOutputAudioFormat));
			}
			break;
		default:
        	LOG_REMOTE(LOG_ERROR, pSource, pLine, "Invalid media source type");
			break;

	}

    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "..successfully opened %s format converter", GetMediaTypeStr().c_str());

	return true;
}

bool MediaSource::FfmpegCloseFormatConverter(string pSource, int pLine)
{
    // free resample context
    switch(mMediaType)
    {
        case MEDIA_AUDIO:
            if (mAudioResampleContext != NULL)
            {
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "    ..releasing %s resample context", GetMediaTypeStr().c_str());
                HM_swr_free(&mAudioResampleContext);
                mAudioResampleContext = NULL;
            }
            if (mResampleBuffer != NULL)
            {
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "    ..releasing %s resample buffer", GetMediaTypeStr().c_str());
                free(mResampleBuffer);
                mResampleBuffer = NULL;
            }
            for (int i = 0; i < MEDIA_SOURCE_MAX_AUDIO_CHANNELS; i++)
            {
                if(mResampleFifo[i] != NULL)
                {
                    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "    ..releasing resample FIFO %d", i);
                    av_fifo_free(mResampleFifo[i]);
                    mResampleFifo[i] = NULL;
                }
            }
            break;
        case MEDIA_VIDEO:
            if (mVideoScalerContext != NULL)
            {
                // free the software scaler context
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "    ..releasing %s scale context", GetMediaTypeStr().c_str());
                sws_freeContext(mVideoScalerContext);
                mVideoScalerContext = NULL;
            }
            break;
        default:
        	LOG_REMOTE(LOG_ERROR, pSource, pLine, "Invalid media source type");
			break;
    }
    return true;
}

bool MediaSource::FfmpegCloseAll(string pSource, int pLine)
{
    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "%s %s source closing..", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());

	if (mMediaSourceOpened)
	{
		mMediaSourceOpened = false;

		// stop A/V recorder
		LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "    ..stopping %s recorder", GetMediaTypeStr().c_str());
        StopRecording();

        if (!FfmpegCloseFormatConverter(pSource, pLine))
        {
            LOG_REMOTE(LOG_ERROR, pSource, pLine, "Failed to close %s format converter", GetMediaTypeStr().c_str());
            return false;
        }

		// Close the codec
		if (mCodecContext != NULL)
		{
		    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "    ..closing %s codec", GetMediaTypeStr().c_str());
	    	mFormatContext->streams[mMediaStreamIndex]->discard = AVDISCARD_ALL;
			avcodec_close(mCodecContext);
			mCodecContext = NULL;
		}else
		{
			LOG_REMOTE(LOG_WARN, pSource, pLine, "Codec context found in invalid state");
			return false;
		}

		// Close the file
		if (mFormatContext != NULL)
		{
		    LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "    ..closing %s format context and releasing codec context", GetMediaTypeStr().c_str());
			HM_avformat_close_input(mFormatContext);
			mFormatContext = NULL;
		}else
		{
			LOG_REMOTE(LOG_WARN, pSource, pLine, "Format context found in invalid state");
			return false;
		}

		LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "...%s source closed", GetMediaTypeStr().c_str());
	}

    return true;
}

bool MediaSource::FfmpegEncodeAndWritePacket(string pSource, int pLine, AVFormatContext *pFormatContext, AVCodecContext *pCodecContext, AVFrame *pInputFrame, bool &pIsKeyFrame, int &pBufferedFrames)
{
    AVPacket            tPacketStruc, *tPacket = &tPacketStruc;
    int                 tEncoderResult;
    int                 tFrameFinished;
    bool                tResult = false;

    // #########################################
    // init. packet structure
    // #########################################
    av_init_packet(tPacket);
    tPacket->data = NULL;
    tPacket->size = 0;

    // #########################################
    // re-encode the frame
    // #########################################
    switch(mMediaType)
    {
        case MEDIA_VIDEO:
                tEncoderResult = HM_avcodec_encode_video2(pCodecContext, tPacket, pInputFrame, &tFrameFinished);
                break;
        case MEDIA_AUDIO:
                tEncoderResult = avcodec_encode_audio2(pCodecContext, tPacket, pInputFrame, &tFrameFinished);
                break;
        default:
                break;
    }

    // #########################################
    // write encoded frame
    // #########################################
    if (tEncoderResult >= 0)
    {
        if (tFrameFinished == 1)
        {
            if (tPacket->flags & AV_PKT_FLAG_KEY)
                pIsKeyFrame = true;
            else
                pIsKeyFrame = false;

            #ifdef MS_DEBUG_ENCODER_PACKETS
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "Writing %s packet..", GetMediaTypeStr().c_str());
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "      ..duration: %d", tPacket->duration);
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "      ..flags: %d", tPacket->flags);
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "      ..pts: %"PRId64" (frame pts: %"PRId64", buffered frames: %d)", tPacket->pts, pInputFrame->pts, pBufferedFrames);
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "      ..dts: %"PRId64"", tPacket->dts);
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "      ..size: %d", tPacket->size);
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "      ..pos: %"PRId64"", tPacket->pos);
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "      ..key frame: %d", pIsKeyFrame);
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "      ..codec delay: %d", pCodecContext->delay);
                LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "      ..codec max. b frames: %d", pCodecContext->max_b_frames);
            #endif

             // distribute the encoded frame
             if (av_write_frame(pFormatContext, tPacket) != 0)
                 LOG_REMOTE(LOG_ERROR, pSource, pLine, "Couldn't write %s frame to file", GetMediaTypeStr().c_str());

             tResult = true;
        }else
        {// video frame was buffered by ffmpeg
            LOG_REMOTE(LOG_VERBOSE, pSource, pLine, "%s frame was buffered in encoder", GetMediaTypeStr().c_str());
            pBufferedFrames++;
        }
    }else
    	if (AVUNERROR(tEncoderResult) != EPERM)
    	{// failure reason is "operation not permitted"
    		LOG_REMOTE(LOG_ERROR, pSource, pLine, "Couldn't re-encode current %s frame %"PRId64" because %s(%d)", GetMediaTypeStr().c_str(), pInputFrame->pts, strerror(AVUNERROR(tEncoderResult)), tEncoderResult);
    	}else
    	{// failure reason is something different
			#ifdef MS_DEBUG_ENCODER_PACKETS
    			LOG_REMOTE(LOG_ERROR, pSource, pLine, "Couldn't re-encode current %s frame %"PRId64" because %s(%d)", GetMediaTypeStr().c_str(), pInputFrame->pts, strerror(AVUNERROR(tEncoderResult)), tEncoderResult);
			#endif
    	}
    av_free_packet(tPacket);

    return tResult;
}
///////////////////////////////////////////////////////////////////////////////

}} //namespace
