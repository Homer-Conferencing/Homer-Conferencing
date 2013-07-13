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
 * Purpose: Implementation of a media source multiplexer
 * Author:  Thomas Volkert
 * Since:   2009-01-04
 */

#include <MediaSourceMuxer.h>
#include <MediaSourceNet.h>
#include <MediaSinkNet.h>
#include <MediaSourceFile.h>
#include <VideoScaler.h>
#include <ProcessStatisticService.h>
#include <HBSocket.h>
#include <HBSystem.h>
#include <RTP.h>
#include <Logger.h>

#include <string>
#include <stdint.h>

using namespace std;
using namespace Homer::Monitor;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// maximum packet size of a reeencoded frame, must not be more than 64 kB - otherwise it can't be used via networks!
#define MEDIA_SOURCE_MUX_STREAM_PACKET_BUFFER_SIZE               MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE

// de/activate MT support during video encoding: ffmpeg supports MT only for encoding
#define MEDIA_SOURCE_MUX_MULTI_THREADED_VIDEO_ENCODING

// video bit rate which is used during streaming as default setting
#define MEDIA_SOURCE_MUX_DEFAULT_VIDEO_BIT_RATE                 (90 * 1024)

// audio bit rate which is used during streaming as default setting
#define MEDIA_SOURCE_MUX_DEFAULT_AUDIO_BIT_RATE                 (256 * 1024)

///////////////////////////////////////////////////////////////////////////////

MediaSourceMuxer::MediaSourceMuxer(MediaSource *pMediaSource):
    MediaSource("Muxer: encoder output")
{
    mSourceType = SOURCE_MUXER;
    mStreamPacketBuffer = (char*)av_malloc(MEDIA_SOURCE_MUX_STREAM_PACKET_BUFFER_SIZE);
    SetOutgoingStream();
    mStreamCodecId = AV_CODEC_ID_NONE;
    mStreamMaxPacketSize = 500;
    mStreamQuality = 20;
    mStreamBitRate = -1;
    mStreamMaxFps = 0;
    mVideoHFlip = false;
    mVideoVFlip = false;
    mMediaSource = pMediaSource;
    if (mMediaSource != NULL)
    	mMediaSources.push_back(mMediaSource);
    for (int i = 0; i < 32; i++)
        mResampleFifo[i] = NULL;
    mCurrentStreamingResX = 0;
    mCurrentStreamingResY = 0;
    mRequestedStreamingResX = 352;
    mRequestedStreamingResY = 288;
    mStreamActivated = true;
    mRelayingSkipAudioSilence = false;
    mRelayingSkipAudioSilenceSkippedChunks = 0;
    mEncoderThreadNeeded = true;
    mEncoderHasKeyFrame = false;
    mEncoderFifo = NULL;
}

MediaSourceMuxer::~MediaSourceMuxer()
{
	LOG(LOG_VERBOSE, "Going to destroy %s muxer", GetMediaTypeStr().c_str());

	if (mMediaSourceOpened)
        mMediaSource->CloseGrabDevice();

	LOG(LOG_VERBOSE, "..stopping %s encoder", GetMediaTypeStr().c_str());
    StopEncoder();

	LOG(LOG_VERBOSE, "..freeing stream packet buffer");
    av_free(mStreamPacketBuffer);
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////
// static call back function for ffmpeg encoder
int MediaSourceMuxer::DistributePacket(void *pOpaque, uint8_t *pBuffer, int pBufferSize)
{
    MediaSourceMuxer *tMuxer = (MediaSourceMuxer*)pOpaque;
    char *tBuffer = (char*)pBuffer;

    // drop write_header packets here
    if (!tMuxer->mEncoderThreadNeeded)
        return pBufferSize;

    // log statistics
    tMuxer->AnnouncePacket(pBufferSize);

    //####################################################################
    // distribute frame among the registered media sinks
    // ###################################################################
    #ifdef MSM_DEBUG_PACKETS
        LOGEX(MediaSourceMuxer, LOG_VERBOSE, "Distribute %s packet of size: %d, chunk number: %d", tMuxer->GetMediaTypeStr().c_str(), pBufferSize, tMuxer->mFrameNumber);
        if (pBufferSize > MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE)
        {
            LOGEX(MediaSourceMuxer, LOG_WARN, "Encoded %s data of %d bytes is too big for network streaming", tMuxer->GetMediaTypeStr().c_str(), pBufferSize);
        }
	    if (pBufferSize > tMuxer->mStreamMaxPacketSize)
	    {
    	    LOGEX(MediaSourceMuxer, LOG_WARN, "Ffmpeg %s packet of %d bytes is bigger than maximum payload size of %d bytes, RTP packetizer will fragment to solve this", tMuxer->GetMediaTypeStr().c_str(), pBufferSize, tMuxer->mStreamMaxPacketSize);
	    }
    #endif

    tMuxer->RelayPacketToMediaSinks(tBuffer, (unsigned int)pBufferSize, tMuxer->mEncoderHasKeyFrame);

    return pBufferSize;
}

///////////////////////////////////////////////////////////////////////////////

bool MediaSourceMuxer::SupportsMuxing()
{
    return true;
}

string MediaSourceMuxer::GetMuxingCodec()
{
	return MediaSource::GetGuiNameFromCodecID(mStreamCodecId);
}

void MediaSourceMuxer::GetMuxingResolution(int &pResX, int &pResY)
{
    pResX = mCurrentStreamingResX;
    pResY = mCurrentStreamingResY;
}

int MediaSourceMuxer::GetMuxingBufferCounter()
{
    int tResult = 0;
    mEncoderFifoAvailableMutex.lock();

    if (mEncoderFifo != NULL)
        tResult = mEncoderFifo->GetUsage();

    mEncoderFifoAvailableMutex.unlock();

    return tResult;
}

int MediaSourceMuxer::GetMuxingBufferSize()
{
    return MEDIA_SOURCE_MUX_INPUT_QUEUE_SIZE_LIMIT;
}

bool MediaSourceMuxer::SupportsRelaying()
{
    return true;
}

int MediaSourceMuxer::GetEncoderBufferedFrames()
{
	return mEncoderBufferedFrames;
}

MediaSource* MediaSourceMuxer::GetMediaSource()
{
    return mMediaSource;
}

// return if something has changed
bool MediaSourceMuxer::SetOutputStreamPreferences(std::string pStreamCodec, int pMediaStreamQuality, int pBitRate, int pMaxPacketSize, bool pDoReset, int pResX, int pResY, bool pRtpActivated, int pMaxFps)
{
    // HINT: returns if something has changed
    bool tResult = false;
    enum AVCodecID tStreamCodecId = GetCodecIDFromGuiName(pStreamCodec);

    pMaxPacketSize -= IP6_HEADER_SIZE; // IPv6 overhead is bigger than IPv4
    pMaxPacketSize -= IP_OPTIONS_SIZE; // IP options size: used for QoS signaling
    pMaxPacketSize -= TCP_HEADER_SIZE; // TCP overhead is bigger than UDP/UDP-Lite
    pMaxPacketSize -= TCP_FRAGMENT_HEADER_SIZE; // TCP fragment header which is used to differentiate the RTP packets (fragments) in a received TCP packet
    pMaxPacketSize -= RTP::GetHeaderSizeMax(tStreamCodecId);
	//pMaxPacketSize -= 32; // additional safety buffer size

    // sanity check for max. packet size
    if (pMaxPacketSize > MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE - 256) //HINT: assume 256 bytes of maximum overhead for additional headers
        pMaxPacketSize = MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE - 256;

    if ((pResX != -1) && (pResY != -1))
    {
        int tResX = pResX;
        int tResY = pResY;

        // limit resolution settings according to the features of video codecs
    	ApplyVideoResolutionToEncoderCodec(tResX, tResX, tStreamCodecId);

        if ((tResX != pResX) || (tResY != pResY))
        {
            if ((pResX != -1) && (pResY != -1))
            	LOG(LOG_WARN, "Codec doesn't support selected video resolution, changed resolution from %d*%d to %d*%d", pResX, pResY, tResX, tResY);
            else
            	LOG(LOG_VERBOSE, "Selected auto-detect resolution %d*%d", tResX, tResY);
            pResX = tResX;
            pResY = tResY;
        }
    }

    if ((mStreamCodecId != tStreamCodecId) ||
   		(mStreamMaxFps != pMaxFps) ||
        (GetRtpActivation() != pRtpActivated) ||
        (mStreamQuality != pMediaStreamQuality) ||
        (mStreamBitRate != pBitRate) ||
        (mStreamMaxPacketSize != pMaxPacketSize) ||
        (mCurrentStreamingResX != pResX) || (mCurrentStreamingResY != pResY))
    {
        LOG(LOG_VERBOSE, "Setting new %s streaming preferences", GetMediaTypeStr().c_str());

        tResult = true;

        // set new quality
        LOG(LOG_VERBOSE, "    ..stream FPS: %d => %d", mStreamMaxFps, pMaxFps);
        mStreamMaxFps = pMaxFps;

        AVCodec *tCodec = avcodec_find_encoder(mStreamCodecId);
        string tCurrentCodecName = "unknown";
        if (tCodec != NULL)
        	tCurrentCodecName = string(tCodec->name);

        // set new codec
        LOG(LOG_VERBOSE, "    ..stream codec: %d(%s) => %d(%s)", mStreamCodecId, tCurrentCodecName.c_str(), tStreamCodecId, pStreamCodec.c_str());
        mStreamCodecId = tStreamCodecId;

        // set the stream's maximum packet size
        LOG(LOG_VERBOSE, "    ..stream max packet size: %d => %d", mStreamMaxPacketSize, pMaxPacketSize);
        mStreamMaxPacketSize = pMaxPacketSize;

        // set new quality
        LOG(LOG_VERBOSE, "    ..stream quality: %d => %d", mStreamQuality, pMediaStreamQuality);
        mStreamQuality = pMediaStreamQuality;

        // set new bit rate
        LOG(LOG_VERBOSE, "    ..stream bit rate: %d => %d", mStreamBitRate, pBitRate);
        mStreamBitRate = pBitRate;

        // set RTP encapsulation state
        LOG(LOG_VERBOSE, "    ..stream rtp encapsulation: %d => %d", GetRtpActivation(), pRtpActivated);
        SetRtpActivation(pRtpActivated);

        // set new streaming resolution
        LOG(LOG_VERBOSE, "    ..stream resolution: %d*%d => %d*%d", mRequestedStreamingResX, mRequestedStreamingResY, pResX, pResY);
        mRequestedStreamingResX = pResX;
        mRequestedStreamingResY = pResY;

        if ((pDoReset) && (mMediaSourceOpened))
        {
            LOG(LOG_VERBOSE, "Do reset now...");

            Reset();
        }
    }else
        LOG(LOG_VERBOSE, "No settings were changed - ignoring");

    return tResult;
}

void MediaSourceMuxer::ApplyVideoResolutionToEncoderCodec(int &pResX, int &pResY, enum AVCodecID pCodec)
{
    switch(pCodec)
    {
        case AV_CODEC_ID_H261: // supports QCIF, CIF
                if (pResX > 176)
                {// CIF
                	pResX = 352;
                	pResY = 288;
                }else
                {// QCIF
                	pResX = 176;
                	pResY = 144;
                }
                LOG(LOG_VERBOSE, "Resolution %d*%d for codec H.261 automatically selected", pResX, pResY);
                break;
        case AV_CODEC_ID_H263:  // supports SQCIF, QCIF, CIF, CIF4,CIF16
                if(pResX > 704)
                {// CIF16
                	pResX = 1408;
                	pResY = 1152;
                }else if (pResX > 352)
                {// CIF 4
                	pResX = 704;
                	pResY = 576;
                }else if (pResX > 176)
                {// CIF
                	pResX = 352;
                	pResY = 288;
                }else if (pResX > 128)
                {// QCIF
                	pResX = 176;
                	pResY = 144;
                }else
                {// SQCIF
                	pResX = 128;
                	pResY = 96;
                }
                LOG(LOG_VERBOSE, "Resolution %d*%d for codec H.263 automatically selected", pResX, pResY);
                break;
        case AV_CODEC_ID_H263P:
                if ((pResX > 2048) || (pResY > 1152))
                {// max. video resolution is 2048x1152
                    pResX = 2048;
                    pResY = 1152;
                }else
                {// everythin is fine, use the source resolution
                }
                break;
        case AV_CODEC_ID_THEORA:
        		pResX = 352;
        		pResY = 288;
                break;
        default:
        		// we use the original resolution
        		break;
    }
}


bool MediaSourceMuxer::OpenVideoMuxer(int pResX, int pResY, float pFps)
{
    int                 tResult;
    AVIOContext       	*tIoContext;
    AVOutputFormat      *tFormat;
    AVCodec             *tCodec;
    AVDictionary        *tOptions = NULL;

    // use the frame rate from the base source in order to make the encoder produce the correct PTS values
    if (mMediaSource != NULL)
    {
        float tFps = mMediaSource->GetOutputFrameRate();
        if (tFps != pFps)
        {
            LOG(LOG_VERBOSE, "Setting the %s muxer frame rate from %.2f to %.2f (from base source)", GetMediaTypeStr().c_str(), pFps, tFps);
            pFps = tFps;
        }
    }

    // update the frame rate again if the application wants to limit the output frame rate
    if (mStreamMaxFps != 0)
    {
        LOG(LOG_VERBOSE, "Setting the %s muxer frame rate from %.2f to %d (from base source)", GetMediaTypeStr().c_str(), pFps, mStreamMaxFps);
        pFps = mStreamMaxFps;
    }

    if (pFps > 29.97)
        pFps = 29.97;
    if (pFps < 5)
        pFps = 5;

    mMediaType = MEDIA_VIDEO;

    if (mStreamBitRate == -1)
        mStreamBitRate = MEDIA_SOURCE_MUX_DEFAULT_VIDEO_BIT_RATE;

    // for better debbuging
    mGrabMutex.AssignName(GetMediaTypeStr() + "MuxerGrab");
    mEncoderFifoAvailableMutex.AssignName(GetMediaTypeStr() + "MuxerEncoderFifo");
    mMediaSourcesMutex.AssignName(GetMediaTypeStr() + "MuxerMediaSources");
    mMediaSinksMutex.AssignName(GetMediaTypeStr() + "MuxerMediaSinks");

    LOG(LOG_VERBOSE, "Going to open %s muxer with resolution %d * %d and %3.2f fps", GetMediaTypeStr().c_str(), pResX, pResY, pFps);

    if (mMediaSourceOpened)
        return false;

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    // build correct IO-context
    if (!CreateIOContext(mStreamPacketBuffer, MEDIA_SOURCE_MUX_STREAM_PACKET_BUFFER_SIZE, NULL, DistributePacket, this, &tIoContext))
    {
    	LOG(LOG_ERROR, "Error during I/O context creation");
    	return false;
    }

    // limit packet size
    tIoContext->max_packet_size = mStreamMaxPacketSize;

    mSourceResX = pResX;
    mSourceResY = pResY;
    mInputFrameRate = pFps;
    mOutputFrameRate = pFps;

    // allocate new format context
    mFormatContext = AV_NEW_FORMAT_CONTEXT();

    // find format
    LOG(LOG_VERBOSE, "Guessing VIDEO format for codec \"%s\"", GetFormatName(mStreamCodecId).c_str());
    tFormat = AV_GUESS_FORMAT(GetFormatName(mStreamCodecId).c_str(), NULL, NULL);
    if (tFormat == NULL)
    {
        LOG(LOG_ERROR, "Invalid suggested video format");

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    LOG(LOG_VERBOSE, "Using format \"%s\" for VIDEO codec %d", tFormat->name, mStreamCodecId);

    // explicit codec selection for H263, otherwise ffmpeg would use the last H263-selection
    if (mStreamCodecId == AV_CODEC_ID_H263P)
        tFormat->video_codec = AV_CODEC_ID_H263P;
    if (mStreamCodecId == AV_CODEC_ID_H263)
        tFormat->video_codec = AV_CODEC_ID_H263;

    // set correct output format
    mFormatContext->oformat = tFormat;
    // set correct IO-context
    mFormatContext->pb = tIoContext;
    // verbose timestamp debugging
    if (LOGGER.GetLogLevel() == LOG_WORLD)
    {
    	LOG(LOG_WARN, "Enabling ffmpeg timestamp debugging");
    	mFormatContext->debug = FF_FDEBUG_TS;
    }

    // allocate new stream structure
    LOG(LOG_VERBOSE, "..allocating new stream");
    mMediaStreamIndex = 0;
    mEncoderStream = HM_avformat_new_stream(mFormatContext, 0);
    mCodecContext = mEncoderStream->codec;
    mCodecContext->codec_id = tFormat->video_codec;
    mCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;

    // add some extra parameters depending on the selected codec
    switch(tFormat->video_codec)
    {
        case AV_CODEC_ID_MPEG2VIDEO:
                        // force low delay
                        if (tCodec->capabilities & CODEC_CAP_DELAY)
                            mCodecContext->flags |= CODEC_FLAG_LOW_DELAY;
                        break;
        case AV_CODEC_ID_H263P:
                        // old codec codext flag CODEC_FLAG_H263P_SLICE_STRUCT
                        av_dict_set(&tOptions, "structured_slices", "1", 0);
                        // old codec codext flag CODEC_FLAG_H263P_UMV
                        av_dict_set(&tOptions, "umv", "1", 0);
                        // old codec codext flag CODEC_FLAG_H263P_AIV
                        av_dict_set(&tOptions, "aiv", "1", 0);
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_MPEG4:
                        mCodecContext->flags |= CODEC_FLAG_4MV | CODEC_FLAG_AC_PRED;
                        break;
    }

    // put sample parameters
    mCodecContext->bit_rate = mStreamBitRate;

    // resolution
    if (((mRequestedStreamingResX == -1) || (mRequestedStreamingResY == -1)) && (mMediaSource != NULL))
    {
    	mCurrentStreamingResX = mSourceResX;
    	mCurrentStreamingResY = mSourceResY;
    	ApplyVideoResolutionToEncoderCodec(mCurrentStreamingResX, mCurrentStreamingResY, mStreamCodecId);
    }else
    {
		mCurrentStreamingResX = mRequestedStreamingResX;
		mCurrentStreamingResY = mRequestedStreamingResY;
    }

    // for H.263+ both width and height must be multiples of 4
    if (mStreamCodecId == AV_CODEC_ID_H263P)
    {
    	mCurrentStreamingResX += 3;
    	mCurrentStreamingResX /= 4;
    	mCurrentStreamingResX *= 4;

    	mCurrentStreamingResY += 3;
    	mCurrentStreamingResY /= 4;
    	mCurrentStreamingResY *= 4;
    }

    // for H.2634 both width and height must be multiples of 2
    if (mStreamCodecId == AV_CODEC_ID_H264)
    {
        mCurrentStreamingResX += 1;
        mCurrentStreamingResX /= 2;
        mCurrentStreamingResX *= 2;

        mCurrentStreamingResY += 1;
        mCurrentStreamingResY /= 2;
        mCurrentStreamingResY *= 2;
    }

    mCodecContext->width = mCurrentStreamingResX;
    mCodecContext->height = mCurrentStreamingResY;

    /*
     * time base: this is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. for fixed-FrameRate content,
     * timebase should be 1/framerate and timestamp increments should be
     * identically to 1.
     */
    // mpeg1/2 codecs support only non-rational frame rates
    if (((tFormat->video_codec == AV_CODEC_ID_MPEG1VIDEO) || (tFormat->video_codec == AV_CODEC_ID_MPEG2VIDEO)) && (mInputFrameRate = 29.97))
    {
        //HACK: pretend a frame rate of 30 fps, the actual frame rate corresponds to the frame rate from the base media source
        mCodecContext->time_base = (AVRational){100, (int)(30 * 100)};
        mEncoderStream->time_base = (AVRational){100, (int)(30 * 100)};
    }else
    {
        mCodecContext->time_base = (AVRational){100, (int)(mInputFrameRate * 100)};
        mEncoderStream->time_base = (AVRational){100, (int)(mInputFrameRate * 100)};
    }
    // set i frame distance: GOP = group of pictures
    if (mStreamCodecId != AV_CODEC_ID_THEORA)
        mCodecContext->gop_size = (100 - mStreamQuality) / 5; // default is 12
    else
        mCodecContext->gop_size = 0; // force GOP size of 0 for THEORA

    mCodecContext->qmin = 1; // default is 2
    mCodecContext->qmax = 2 +(100 - mStreamQuality) / 4; // default is 31

    // set max. packet size for RTP based packets
    //HINT: don't set if we use H261, otherwise ffmpeg internal functions in mpegvideo_enc.c (MPV_*) would get confused because H261 support is missing in ffmpeg's RTP support
    //TODO: fix packet size limitation here, ffmpegs lacks support for RTP encaps. for H.261 based video streams
    if (tFormat->video_codec != AV_CODEC_ID_H261)
        mCodecContext->rtp_payload_size = mStreamMaxPacketSize;
    else
        RTP::SetH261PayloadSizeMax(mStreamMaxPacketSize);

    // set pixel format
    if (tFormat->video_codec == AV_CODEC_ID_MJPEG)
        mCodecContext->pix_fmt = PIX_FMT_YUVJ420P;
    else
        mCodecContext->pix_fmt = PIX_FMT_YUV420P;

    // activate ffmpeg internal fps emulation
    //mCodecContext->rate_emu = 1;

    // some formats want stream headers to be separate, but this produces some very small packets!
    if(mFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
        mCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

    // allow ffmpeg its speedup tricks
    mCodecContext->flags2 |= CODEC_FLAG2_FAST;

    // Dump information about device file
    av_dump_format(mFormatContext, mMediaStreamIndex, "MediaSourceMuxer (video)", true);

    // Find the encoder for the video stream
    LOG(LOG_VERBOSE, "..finding video encoder");
    if ((tCodec = avcodec_find_encoder(tFormat->video_codec)) == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find a fitting video codec");
        // free codec and stream 0
        av_freep(&mEncoderStream->codec);
        av_freep(&mEncoderStream);

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    #ifdef MEDIA_SOURCE_MUX_MULTI_THREADED_VIDEO_ENCODING
        if (tCodec->capabilities & (CODEC_CAP_FRAME_THREADS | CODEC_CAP_SLICE_THREADS))
        {// threading supported
            // active multi-threading per default for the video encoding: leave two cpus for concurrent tasks (video grabbing/decoding, audio tasks)
            av_dict_set(&tOptions, "threads", "auto", 0);

            // trigger MT usage during video encoding
            int tThreadCount = System::GetMachineCores() - 2;
            if (tThreadCount > 1)
                mCodecContext->thread_count = tThreadCount;
        }else
        {// threading not supported
            LOG(LOG_WARN, "Multi-threading not supported for %s codec %s", GetMediaTypeStr().c_str(), tCodec->name);
        }
    #endif

    // Open codec
    LOG(LOG_VERBOSE, "..opening video codec");
    if ((tResult = HM_avcodec_open(mCodecContext, tCodec, &tOptions)) < 0)
    {
        LOG(LOG_WARN, "Couldn't open video codec %s because \"%s\". Will try to open the video open codec without options and with disabled MT..", tCodec->name, strerror(AVUNERROR(tResult)));

        // maybe the encoder doesn't support multi-threading?
        mCodecContext->thread_count = 1;
        AVDictionary *tNullOptions = NULL;
        if ((tResult = HM_avcodec_open(mCodecContext, tCodec, &tNullOptions)) < 0)
        {
            LOG(LOG_ERROR, "Couldn't open video codec because \"%s\".", strerror(AVUNERROR(tResult)));

            // free codec and stream 0
            av_freep(&mEncoderStream->codec);
            av_freep(&mEncoderStream);

            // Close the format context
            av_free(mFormatContext);

            return false;
        }
    }

    if (tCodec->capabilities & CODEC_CAP_DELAY)
        LOG(LOG_VERBOSE, "%s encoder output might be delayed for %s codec", GetMediaTypeStr().c_str(), mCodecContext->codec->name);

    // init transcoder FIFO based for RGB32 pictures
    StartEncoder();

    //######################################################
    //### give some verbose output
    //######################################################
    mStreamMaxFps_LastFrame_Timestamp = Time::GetTimeStamp();
    MarkOpenGrabDeviceSuccessful();
    LOG(LOG_INFO, "    ..max packet size: %d bytes", mFormatContext->pb->max_packet_size);
    LOG(LOG_INFO, "  stream...");
    LOG(LOG_INFO, "    ..AV stream context at: %p", mEncoderStream);
    LOG(LOG_INFO, "    ..AV stream codec is: %s(%d)", mEncoderStream->codec->codec->name, mEncoderStream->codec->codec_id);
    LOG(LOG_INFO, "    ..AV stream codec context at: 0x%p", mEncoderStream->codec);
    LOG(LOG_INFO, "    ..AV stream codec codec context at: 0x%p", mEncoderStream->codec->codec);
    if (mRtpActivated)
        LOG(LOG_INFO, "    ..rtp encapsulation: yes");
    else
        LOG(LOG_INFO, "    ..rtp encapsulation: no");
    LOG(LOG_INFO, "    ..max. packet size: %d bytes", mStreamMaxPacketSize);

    return true;
}

bool MediaSourceMuxer::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    bool tResult = false;

    if (pFps > 29.97)
        pFps = 29.97;
    if (pFps < 5)
        pFps = 5;

    // set media type early to have verbose debug outputs in case of failures
    mMediaType = MEDIA_VIDEO;

    LOG(LOG_VERBOSE, "Going to open %s grab device with %3.2f fps", GetMediaTypeStr().c_str(), pFps);

    // first open hardware video source
    if (mMediaSource != NULL)
    {
    	tResult = mMediaSource->OpenVideoGrabDevice(pResX, pResY, pFps);
		if (!tResult)
			return false;
		mInputFrameRate = mMediaSource->GetInputFrameRate();
		mOutputFrameRate = mMediaSource->GetOutputFrameRate();
    }

    if (mMediaSourceOpened)
        return false;

    // afterwards open the muxer, independent from the open state of the local video
    if (mMediaSource != NULL)
    	OpenVideoMuxer(pResX, pResY, pFps);
    else
    	tResult = OpenVideoMuxer(pResX, pResY, pFps);

    return tResult;
}

bool MediaSourceMuxer::OpenAudioMuxer(int pSampleRate, int pChannels)
{
    int                 tResult;
    AVIOContext       	*tIoContext;
    AVOutputFormat      *tFormat;
    AVCodec             *tCodec;

    mMediaType = MEDIA_AUDIO;

    if (mStreamBitRate == -1)
        mStreamBitRate = MEDIA_SOURCE_MUX_DEFAULT_AUDIO_BIT_RATE;

    // invert meaning of I/O state
    mInputAudioChannels = pChannels;
    mInputAudioSampleRate = pSampleRate;

    // for better debbuging
    mGrabMutex.AssignName(GetMediaTypeStr() + "MuxerGrab");
    mEncoderFifoAvailableMutex.AssignName(GetMediaTypeStr() + "MuxerEncoderFifo");
    mMediaSourcesMutex.AssignName(GetMediaTypeStr() + "MuxerMediaSources");
    mMediaSinksMutex.AssignName(GetMediaTypeStr() + "MuxerMediaSinks");

    LOG(LOG_VERBOSE, "Going to open %s-muxer", GetMediaTypeStr().c_str());

    if (mMediaSourceOpened)
        return false;

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);

    // build correct IO-context
    if (!CreateIOContext(mStreamPacketBuffer, MEDIA_SOURCE_MUX_STREAM_PACKET_BUFFER_SIZE, NULL, DistributePacket, this, &tIoContext))
    {
    	LOG(LOG_ERROR, "Error during I/O context creation");
    	return false;
    }

    // limit packet size
    tIoContext->max_packet_size = mStreamMaxPacketSize;

    // allocate new format context
    mFormatContext = AV_NEW_FORMAT_CONTEXT();

    // find format
    LOG(LOG_VERBOSE, "Guessing AUDIO format for codec \"%s\"", GetFormatName(mStreamCodecId).c_str());
    tFormat = AV_GUESS_FORMAT(GetFormatName(mStreamCodecId).c_str(), NULL, NULL);
    if (tFormat == NULL)
    {
        LOG(LOG_ERROR, "Invalid suggested audio format for codec %d", mStreamCodecId);

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    LOG(LOG_VERBOSE, "Using format \"%s\" for AUDIO codec %d", tFormat->name, mStreamCodecId);

    // set correct output format
    mFormatContext->oformat = tFormat;
    // set correct IO-context
    mFormatContext->pb = tIoContext;
    // verbose timestamp debugging
    if (LOGGER.GetLogLevel() == LOG_WORLD)
    {
    	LOG(LOG_WARN, "Enabling ffmpeg timestamp debugging");
    	mFormatContext->debug = FF_FDEBUG_TS;
    }

    // allocate new stream structure
    mMediaStreamIndex = 0;
    mEncoderStream = HM_avformat_new_stream(mFormatContext, 0);
    mCodecContext = mEncoderStream->codec;
    mCodecContext->codec_id = tFormat->audio_codec;
    mCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
    switch(mCodecContext->codec_id)
    {
		case AV_CODEC_ID_ADPCM_G722:
			mOutputAudioChannels = 1;
			mOutputAudioSampleRate = 16000;
			mCodecContext->sample_fmt = AV_SAMPLE_FMT_S16; // packed
			break;
		case AV_CODEC_ID_AMR_NB:
			mOutputAudioChannels = 1;
			mCodecContext->bit_rate = 7950; // force to 7.95kHz , limit is given by libopencore_amrnb
			mOutputAudioSampleRate = 8000; //force 8 kHz for AMR-NB
			mCodecContext->sample_fmt = AV_SAMPLE_FMT_S16; // packed
			break;
		case AV_CODEC_ID_GSM:
		case AV_CODEC_ID_PCM_ALAW:
		case AV_CODEC_ID_PCM_MULAW:
			mOutputAudioChannels = 1;
			mOutputAudioSampleRate = 8000;
			mCodecContext->sample_fmt = AV_SAMPLE_FMT_S16; // packed
			break;
    	case AV_CODEC_ID_PCM_S16BE:
			mOutputAudioChannels = 2;
			mOutputAudioSampleRate = 44100;
			mCodecContext->sample_fmt = AV_SAMPLE_FMT_S16; // packed
			break;
        case AV_CODEC_ID_MP3:
			mOutputAudioChannels = pChannels;
			mOutputAudioSampleRate = pSampleRate;
			mCodecContext->sample_fmt = AV_SAMPLE_FMT_S16P; // planar
			mCodecContext->bit_rate = mStreamBitRate; // streaming rate
        	break;
		default:
			mOutputAudioChannels = 2;
			mOutputAudioSampleRate = 44100;
			mCodecContext->sample_fmt = AV_SAMPLE_FMT_S16; // packed
			break;
    }
	mEncoderStream->time_base = (AVRational){1, mOutputAudioSampleRate};
	mCodecContext->channels = mOutputAudioChannels;
	mCodecContext->channel_layout = av_get_default_channel_layout(mOutputAudioChannels);
    mCodecContext->sample_rate = mOutputAudioSampleRate;
    // set max. packet size for RTP based packets
    mCodecContext->rtp_payload_size = mStreamMaxPacketSize;

    // some formats want stream headers to be separate, but this produces some very small packets!
    if(mFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
        mCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

    // allow ffmpeg its speedup tricks
    mCodecContext->flags2 |= CODEC_FLAG2_FAST;

    // Dump information about device file
    av_dump_format(mFormatContext, mMediaStreamIndex, "MediaSourceMuxer (audio)", true);

    // Find the encoder for the audio stream
    if((tCodec = avcodec_find_encoder(tFormat->audio_codec)) == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find a fitting audio codec");
        // free codec and stream 0
        av_freep(&mEncoderStream->codec);
        av_freep(&mEncoderStream);

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    // Inform the codec that we can handle truncated bitstreams -- i.e.,
    // bitstreams where sample boundaries can fall in the middle of packets
//    if(tCodec->capabilities & CODEC_CAP_TRUNCATED)
//        mCodecContext->flags |= CODEC_FLAG_TRUNCATED;

    // allow ffmpeg its speedup tricks
    mCodecContext->flags2 |= CODEC_FLAG2_FAST;

    // Open codec
    if ((tResult = HM_avcodec_open(mCodecContext, tCodec, NULL)) < 0)
    {
        LOG(LOG_ERROR, "Couldn't open audio codec %s because \"%s\".", tCodec->name, strerror(AVUNERROR(tResult)));
        // free codec and stream 0
        av_freep(&mEncoderStream->codec);
        av_freep(&mEncoderStream);

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    // fix frame size of 0 for some audio codecs
    if (mCodecContext->frame_size < 32)
    	mCodecContext->frame_size = MEDIA_SOURCE_SAMPLES_PER_BUFFER;

    mOutputAudioFormat = mCodecContext->sample_fmt;

    // update the real frame rate depending on the actual encoder sample rate and the encoder frame size
    mOutputFrameRate = (float)mOutputAudioSampleRate /* usually 44100 samples per second */ / mCodecContext->frame_size /* usually 1024 samples per frame */;

    // init transcoder FIFO based for 2048 samples with 16 bit and 2 channels, more samples are never produced by a media source per grabbing cycle
    StartEncoder();

    MarkOpenGrabDeviceSuccessful();
    LOG(LOG_INFO, "    ..max packet size: %d bytes", mFormatContext->pb->max_packet_size);
    LOG(LOG_INFO, "  stream...");
    if (mRtpActivated)
        LOG(LOG_INFO, "    ..rtp encapsulation: yes");
    else
        LOG(LOG_INFO, "    ..rtp encapsulation: no");
    LOG(LOG_INFO, "    ..max. packet size: %d bytes", mStreamMaxPacketSize);

    return true;
}


bool MediaSourceMuxer::OpenAudioGrabDevice(int pSampleRate, int pChannels)
{
    bool tResult = false;

    // set media type early to have verbose debug outputs in case of failures
    mMediaType = MEDIA_AUDIO;
    mOutputAudioChannels = pChannels;
    mOutputAudioSampleRate = pSampleRate;

    LOG(LOG_VERBOSE, "Going to open %s grab device", GetMediaTypeStr().c_str());
    LOG(LOG_VERBOSE, "..output sample rate: %d", mOutputAudioSampleRate);
    LOG(LOG_VERBOSE, "..output channels: %d", mOutputAudioChannels);

    // first open hardware video source
    if (mMediaSource != NULL)
    {
		tResult = mMediaSource->OpenAudioGrabDevice(pSampleRate, pChannels);
		if (!tResult)
			return false;
        mInputFrameRate = mMediaSource->GetInputFrameRate();
        mOutputFrameRate = mMediaSource->GetOutputFrameRate();
    }

    if (mMediaSourceOpened)
        return false;

    // afterwards open the muxer, independent from the open state of the local video
    if (mMediaSource != NULL)
    	OpenAudioMuxer(pSampleRate, pChannels);
    else
    	tResult = OpenAudioMuxer(pSampleRate, pChannels);

    return tResult;
}

bool MediaSourceMuxer::CloseMuxer()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close %s-muxer", GetMediaTypeStr().c_str());

    // HINT: no mMediaSinksMutex usage because StopEncoder will stop all media sink usage and this CloseMuxer doesn't change the registered media sinks

    if (mMediaSourceOpened)
    {
        mMediaSourceOpened = false;

        // make sure we can free the memory structures
        StopEncoder();

        LOG(LOG_VERBOSE, "..closing %s codec", GetMediaTypeStr().c_str());

        // Close the codec
    	mEncoderStream->discard = AVDISCARD_ALL;
        avcodec_close(mCodecContext);

        // free codec and stream 0
        av_freep(&mEncoderStream->codec);
        av_freep(mEncoderStream);

        // Close the format context
        av_free(mFormatContext);

        LOG(LOG_INFO, "...%s-muxer closed", GetMediaTypeStr().c_str());

        tResult = true;
    }else
        LOG(LOG_INFO, "...%s-muxer wasn't opened", GetMediaTypeStr().c_str());

    ResetPacketStatistic();

    mFrameNumber = 0;
    mRelayingSkipAudioSilenceSkippedChunks = 0;

    return tResult;
}

bool MediaSourceMuxer::CloseGrabDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close %s grab device", GetMediaTypeStr().c_str());

    if (mMediaSourceOpened)
    {
        CloseMuxer();

        tResult = true;
    }else
    	LOG(LOG_INFO, "%s-muxer is already closed", GetMediaTypeStr().c_str());

    if (mMediaSource != NULL)
    	tResult = mMediaSource->CloseGrabDevice() && tResult;
    else
    	LOG(LOG_INFO, "No %s source available", GetMediaTypeStr().c_str());

    mGrabbingStopped = false;

    return tResult;
}

static int sArrowWidth = 8;
static int sArrowHeight = 16;
static char sArrow [8 * 16] = { 1,0,0,0,0,0,0,0,
                                1,1,0,0,0,0,0,0,
                                1,2,1,0,0,0,0,0,
                                1,2,2,1,0,0,0,0,
                                1,2,2,2,1,0,0,0,
                                1,2,2,2,2,1,0,0,
                                1,2,2,2,2,2,1,0,
                                1,2,2,2,2,2,2,1,
                                1,2,2,2,2,1,1,1,
                                1,1,1,2,2,1,0,0,
                                0,0,1,2,2,1,0,0,
                                0,0,0,1,2,2,1,0,
                                0,0,0,1,2,2,1,0,
                                0,0,0,1,2,2,1,0,
                                0,0,0,0,1,1,1,0,
                                0,0,0,0,0,0,0,0 };

void SetPixel(char *pBuffer, int pWidth, int pHeight, int pX, int pY, char pRed, char pGreen, char pBlue)
{
    if ((pX >= 0) && (pX < pWidth) && (pY >= 0) && (pY < pHeight))
    {
        unsigned long tOffset = (pWidth * pY + pX) * 4;
        char *tPixel = pBuffer + tOffset;
        //LOGEX(MediaSourceMuxer, LOG_WARN, "Setting pixel at rel. pos.: %d, %d; offset: %u; pixel addres is %p (buffer address is %p)", pX, pY, tOffset, tPixel, pBuffer);
        *tPixel = pRed;
        tPixel++;
        *tPixel = pGreen;
        tPixel++;
        *tPixel = pBlue;
        tPixel++;
        *tPixel = 0;
    }
}

static void DrawArrow(char *pBuffer, int pWidth, int pHeight, int pPosX, int pPosY)
{
    int tXScale = pWidth / 400 + 1;
    int tYScale = pHeight / 400 + 1;

    for (int y = 0; y < sArrowHeight; y++)
    {
        for (int x = 0; x < sArrowWidth; x++)
        {
            for (int ys = 0; ys < tYScale; ys++)
            {
                for (int xs = 0; xs < tXScale; xs++)
                {
                    switch(sArrow[y * sArrowWidth + x])
                    {
                        case 0:
                            break;
                        case 1:
                            SetPixel(pBuffer, pWidth, pHeight, pPosX + x * tXScale + xs, pPosY + y * tYScale + ys, 0, 0, 0);
                            break;
                        case 2:
                            SetPixel(pBuffer, pWidth, pHeight, pPosX + x * tXScale + xs, pPosY + y * tYScale + ys , 255, 255, 255);
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }
}

int MediaSourceMuxer::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    MediaSinks::iterator     tIt;
    int                         tResult;
    AVFrame                     *tRGBFrame;
    AVFrame                     *tYUVFrame;
    AVPacket                    tPacket;
    int                         tFrameSize;

    #ifdef MSM_DEBUG_GRABBING
        LOG(LOG_VERBOSE, "Trying to grab a new %s chunk", GetMediaTypeStr().c_str());
    #endif

    // lock grabbing
    mGrabMutex.lock();

    //HINT: maybe unsafe, buffer could be freed between call and mutex lock => task of the application to prevent this
    if (pChunkBuffer == NULL)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("grab " + GetMediaTypeStr() + " buffer is NULL");

        return -1;
    }

    if (mGrabbingStopped)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed(GetMediaTypeStr() + " source is paused");

        return -1;
    }

    if (mMediaSource == NULL)
    {
    	// unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed(GetMediaTypeStr() + " base source is undefined");

        return -1;
    }

    //####################################################################
    // get frame from the original media source
    // ###################################################################
    tResult = mMediaSource->GrabChunk(pChunkBuffer, pChunkSize, pDropChunk);
    #ifdef MSM_DEBUG_GRABBING
        if (!pDropChunk)
        {
            switch(mMediaType)
            {
                        case MEDIA_VIDEO:
                                LOG(LOG_VERBOSE, "Got result %d with %d bytes at 0x%p from original video source with dropping = %d", tResult, pChunkSize, pChunkBuffer, pDropChunk);
                                break;
                        case MEDIA_AUDIO:
                                LOG(LOG_VERBOSE, "Got result %d with %d bytes at 0x%p from original audio source with dropping = %d", tResult, pChunkSize, pChunkBuffer, pDropChunk);
                                break;
                        default:
                                LOG(LOG_VERBOSE, "Unknown media type");
                                break;
            }
        }
    #endif

    //####################################################################
    // horizontal/vertical picture flipping
    // ###################################################################
    if (mMediaType == MEDIA_VIDEO)
    {
        if (mVideoVFlip)
        {
            int tRowLength = mSourceResX * 4;
            char *tPicture = (char*)pChunkBuffer;
            char *tRowBuffer = (char*)malloc(tRowLength);
            int tRowMax = mSourceResY / 2;
            char *tUpperRow = tPicture;
            char *tLowerRow = tPicture + (tRowLength * (mSourceResY - 1));

            for (int tRowCount = 0; tRowCount < tRowMax; tRowCount++)
            {
                // save first line
              memcpy(tRowBuffer, tUpperRow, tRowLength);
              // first row = last row
              memcpy(tUpperRow, tLowerRow, tRowLength);
              // last row = saved row
              memcpy(tLowerRow, tRowBuffer, tRowLength);

              tUpperRow += tRowLength;
              tLowerRow -= tRowLength;
            }
            free(tRowBuffer);
        }
        if (mVideoHFlip)
        {
            unsigned int *tPixels = (unsigned int*)pChunkBuffer;
            unsigned int *tOrgPixels = (unsigned int*)pChunkBuffer;
            unsigned int tPixelBuffer;
            int tColumnMax = mSourceResX / 2;

            for (int tRowCount = 0; tRowCount < mSourceResY; tRowCount++)
            {
                for (int tColumnCount = 0; tColumnCount < tColumnMax ; tColumnCount++)
                {
                    //LOG(LOG_ERROR, "Column count: %d", tColumnCount);
                    //LOG(LOG_ERROR, "Right: %d", mSourceResX - tColumnCount - 1);
                    tPixelBuffer = tPixels[tColumnCount];
                    //LOG(LOG_ERROR, "Righter: %u", (unsigned long)&tPixels[mSourceResX - tColumnCount - 1]);
                    tPixels[tColumnCount] = tPixels[mSourceResX - tColumnCount - 1];
                    tPixels[mSourceResX - tColumnCount - 1] = tPixelBuffer;
                }
                tPixels += (unsigned int)mSourceResX;
                //LOG(LOG_ERROR, "#### Row: %d Pixels: %u", tRowCount, (unsigned int)(tPixels - tOrgPixels));
            }
        }
    }

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed(GetMediaTypeStr() + " muxer is closed");

        return tResult;
    }

    // lock
    mMediaSinksMutex.lock();

    int tMediaSinks = mMediaSinks.size();

    // unlock
    mMediaSinksMutex.unlock();

    //####################################################################
    // live marker - OSD
    //####################################################################
    if ((mMediaType == MEDIA_VIDEO) && (mMarkerActivated))
    {
        DrawArrow((char*)pChunkBuffer, mSourceResX, mSourceResY, mMarkerRelX * mSourceResX / 100, mMarkerRelY * mSourceResY / 100);
    }

    //####################################################################
    // reencode frame and send it to the registered media sinks
    // limit the outgoing stream FPS to the defined maximum FPS value
    // ###################################################################
    mEncoderFifoAvailableMutex.lock();

    if ((BelowMaxFps(tResult) /* we have to call this function continuously */) && (mStreamActivated) && (!pDropChunk) && (tResult >= 0) && (pChunkSize > 0) && (tMediaSinks) && (mEncoderFifo != NULL))
    {
		// we relay this chunk to all registered media sinks based on the dedicated relay thread
		int64_t tTime = Time::GetTimeStamp();

		int64_t tNtpTime = (int64_t)RTP::GetNtpTime();

		// set encoder start time in order to be able to support variable output frame rates
		if (mFrameNumber == 0)
			mEncoderStartTime = tNtpTime;

		mEncoderFifo->WriteFifo((char*)pChunkBuffer, pChunkSize, tNtpTime);
		#ifdef MSM_DEBUG_TIMING
			int64_t tTime2 = Time::GetTimeStamp();
			//LOG(LOG_VERBOSE, "Writing %d bytes to Encoder-FIFO took %"PRId64" us", pChunkSize, tTime2 - tTime);
		#endif
    }

    mEncoderFifoAvailableMutex.unlock();

    // unlock grabbing
    mGrabMutex.unlock();

    // acknowledge success
    MarkGrabChunkSuccessful(tResult);

    return tResult;
}

//HINT: call this function continuously !
bool MediaSourceMuxer::BelowMaxFps(int pFrameNumber)
{
    int64_t tCurrentTime = Time::GetTimeStamp();

    if (mStreamMaxFps != 0)
    {
        int64_t tTimeDiffToLastFrame = tCurrentTime - mStreamMaxFps_LastFrame_Timestamp;
        int64_t tTimeDiffTreshold = 1000*1000 / mStreamMaxFps;
        int64_t tTimeDiffForNextFrame = tTimeDiffToLastFrame - tTimeDiffTreshold;
        #ifdef MSM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Checking max. FPS(%d) for frame number %d: %"PRId64" < %"PRId64" => %s", mStreamMaxFps, pFrameNumber, tTimeDiffToLastFrame, tTimeDiffTreshold, (tTimeDiffToLastFrame < tTimeDiffTreshold) ? "yes" : "no");
        #endif

        // time for a new frame?
        if (tTimeDiffForNextFrame > 0)
        {
            mStreamMaxFps_LastFrame_Timestamp = tCurrentTime;

            //LOG(LOG_VERBOSE, "Last frame timestamp: %"PRId64"(%"PRId64") , %"PRId64", %"PRId64"", tCurrentTime, mStreamMaxFps_LastFrame_Timestamp, tTimeDiffForNextFrame, mStreamMaxFps_LastFrame_Timestamp - tTimeDiffForNextFrame);

            // correct reference timestamp for last frame by the already passed time for the next frame
            mStreamMaxFps_LastFrame_Timestamp -= tTimeDiffForNextFrame;
            return true;
        }
    }else
    {
        mStreamMaxFps_LastFrame_Timestamp = tCurrentTime;
        return true;
    }

    return false;
}

int64_t MediaSourceMuxer::CalculateEncoderPts(int pFrameNumber)
{
    int64_t tResult = 0;

    switch(mMediaType)
    {
        case MEDIA_VIDEO:
                    tResult = pFrameNumber * 1000 / GetOutputFrameRate(); // frame number * time between frames
                    break;
        case MEDIA_AUDIO:
                    tResult = pFrameNumber * mCodecContext->frame_size; // = how many samples are already passed?
                    break;
        default:
                    break;
    }

    return tResult;
}

void MediaSourceMuxer::StartEncoder()
{
    LOG(LOG_VERBOSE, "Starting %s transcoder", GetMediaTypeStr().c_str());

    // start transcoder main loop
    StartThread();

    while(!mEncoderThreadNeeded)
    {
        LOG(LOG_VERBOSE, "Waiting for the start of %s transcoding thread", GetMediaTypeStr().c_str());
        Thread::Suspend(25 * 1000);
    }
}

void MediaSourceMuxer::StopEncoder()
{
    int tSignalingRound = 0;

    LOG(LOG_VERBOSE, "Stopping %s transcoder", GetMediaTypeStr().c_str());

    if (mEncoderFifo != NULL)
    {
        // tell transcoder thread it isn't needed anymore
        mEncoderThreadNeeded = false;

        // wait for termination of transcoder thread
        do
        {
            if(tSignalingRound > 0)
                LOG(LOG_WARN, "Signaling attempt %d to stop transcoder", tSignalingRound);
            tSignalingRound++;

            // write fake data to awake transcoder thread as long as it still runs
            mEncoderFifoState.lock();
            if (mEncoderFifo != NULL)
            	mEncoderFifo->WriteFifo(NULL, 0, 0);
            mEncoderFifoState.unlock();

            Thread::Suspend(25 * 1000);
        }while(IsRunning());
    }

    LOG(LOG_VERBOSE, "%s encoder stopped", GetMediaTypeStr().c_str());
}

void MediaSourceMuxer::ResetEncoderBuffers()
{
    mEncoderSeekMutex.lock();

    // flush ffmpeg internal buffers
    LOG(LOG_VERBOSE, "Reseting %s encoder internal buffers after seeking in input stream", GetMediaTypeStr().c_str());
    avcodec_flush_buffers(mCodecContext);

    // reset the library internal frame FIFO
    LOG(LOG_VERBOSE, "Reseting %s encoder internal FIFO after seeking in input stream", GetMediaTypeStr().c_str());
    if (mEncoderFifo != NULL)
        mEncoderFifo->ClearFifo();

    if (mMediaType == MEDIA_AUDIO)
    {
        for (int i = 0; i < MEDIA_SOURCE_MAX_AUDIO_CHANNELS; i++)
        {
            if ((mResampleFifo[i] != NULL) && (av_fifo_size(mResampleFifo[i]) > 0))
            {
                LOG(LOG_VERBOSE, "Reseting %s decoder internal buffers resample FIFO after seeking in input stream", GetMediaTypeStr().c_str());
                av_fifo_drain(mResampleFifo[i], av_fifo_size(mResampleFifo[i]));
            }
        }
    }

    // reset buffer counter
    mEncoderBufferedFrames = 0;

    mEncoderSeekMutex.unlock();
}

void* MediaSourceMuxer::Run(void* pArgs)
{
    char                *tBuffer;
    int                 tBufferSize;
    int                 tFifoEntry = 0;
    int                 tResult;
    AVFrame             *tYUVFrame, *tAudioFrame;
    AVPacket            tPacketStruc, *tPacket = &tPacketStruc;
    int                 tEncoderResult;
    int                 tChunkBufferSize = 0;
    uint8_t             *tChunkBuffer;
    VideoScaler         *tVideoScaler = NULL;
    int                 tFrameFinished = 0;
    int64_t				tLastInputFrameTimestamp = -1;
    int64_t             tInputFrameTimestamp = 0;
    int64_t				tOutputFrameTimestamp = 0;
    int64_t				tVideoEncoderFrameTimestamp = 0;
    int64_t				tLastVideoEncoderFrameTimestamp = 0;

    LOG(LOG_WARN, ">>>>>>>>>>>>>>>> %s-Encoding thread for %s media source started", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());

    switch(mMediaType)
    {
        case MEDIA_VIDEO:
            SVC_PROCESS_STATISTIC.AssignThreadName("Video-Encoder(" + GetFormatName(mStreamCodecId) + ")");

            // Allocate video frame for YUV format
            if ((tYUVFrame = AllocFrame()) == NULL)
                LOG(LOG_ERROR, "Out of video memory in avcodec_alloc_frame()");

            mEncoderChunkBuffer = (char*)av_malloc(MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE);
            if (mEncoderChunkBuffer == NULL)
                LOG(LOG_ERROR, "Out of video memory for encoder chunk buffer");

            // create video scaler
            LOG(LOG_VERBOSE, "..encoder thread starts scaler thread..");
            tVideoScaler = new VideoScaler("Video-Encoder(" + GetFormatName(mStreamCodecId) + ")");
            if(tVideoScaler == NULL)
                LOG(LOG_ERROR, "Invalid video scaler instance, possible out of memory");

            tVideoScaler->StartScaler(MEDIA_SOURCE_MUX_INPUT_QUEUE_SIZE_LIMIT, mSourceResX, mSourceResY, PIX_FMT_RGB32, mCurrentStreamingResX, mCurrentStreamingResY, mCodecContext->pix_fmt);
            LOG(LOG_VERBOSE, "..video scaler thread started..");

            mEncoderFifoAvailableMutex.lock();

            // set the video scaler as FIFO for the encoder
            mEncoderFifo = tVideoScaler;

            mEncoderFifoAvailableMutex.unlock();

            break;
        case MEDIA_AUDIO:
            SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Encoder(" + GetFormatName(mStreamCodecId) + ")");

            // Allocate video frame for YUV format
            if ((tAudioFrame = AllocFrame()) == NULL)
                LOG(LOG_ERROR, "Out of video memory in avcodec_alloc_frame()");

            mEncoderChunkBuffer = (char*)av_malloc(MEDIA_SOURCE_SAMPLES_MULTI_BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
            if (mEncoderChunkBuffer == NULL)
                LOG(LOG_ERROR, "Out of memory for encoder chunk buffer");

            mEncoderFifoAvailableMutex.lock();

            mEncoderFifo = new MediaFifo(MEDIA_SOURCE_MUX_INPUT_QUEUE_SIZE_LIMIT, MEDIA_SOURCE_SAMPLES_MULTI_BUFFER_SIZE * 2, "AUDIO-Encoder");
            if (mEncoderFifo == NULL)
                LOG(LOG_ERROR, "Out of memory for encoder FIFO");

            mEncoderFifoAvailableMutex.unlock();

            break;
        default:
            SVC_PROCESS_STATISTIC.AssignThreadName("Encoder(" + GetFormatName(mStreamCodecId) + ")");
            break;
    }

    if (!OpenFormatConverter())
        LOG(LOG_ERROR, "Failed to open %s format converter", GetMediaTypeStr().c_str());

    // check the actually used bit rate, does it matches the desired one?
    if (mStreamBitRate != mCodecContext->bit_rate)
        LOG(LOG_WARN, "%s codec adapted encoder bit rate from %d to %d", GetMediaTypeStr().c_str(), mStreamBitRate, mCodecContext->bit_rate);

    // allocate streams private data buffer and write the streams header, if any
    if ((tResult = avformat_write_header(mFormatContext, NULL)) < 0)
        LOG(LOG_ERROR, "Couldn't write %s codec header because \"%s\".", GetMediaTypeStr().c_str(), strerror(AVUNERROR(tResult)));

    // set marker to "active"
    mEncoderThreadNeeded = true;

    mFrameNumber = 0;
    mEncoderStartTime = 0;

    // trigger an avcodec_flush_buffers()
    ResetEncoderBuffers();

    LOG(LOG_WARN, "================ Entering main %s encoding loop for %s media source", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());

    while(mEncoderThreadNeeded)
    {
		#ifdef MSM_DEBUG_TIMING
			LOG(LOG_VERBOSE, "%s-encoder loop", GetMediaTypeStr().c_str());
		#endif
        if (mEncoderFifo != NULL)
        {
            //####################################################################
            //### get next frame data
            //###################################################################
            tFifoEntry = mEncoderFifo->ReadFifoExclusive(&tBuffer, tBufferSize, tInputFrameTimestamp /* NTP time */);
            if ((tLastInputFrameTimestamp != -1) && (tInputFrameTimestamp != 0) && (tInputFrameTimestamp < tLastInputFrameTimestamp))
            	LOG(LOG_WARN, "Input %s frame timestamp is too low: %"PRId64" <= %"PRId64", diff: %"PRId64, GetMediaTypeStr().c_str(), tInputFrameTimestamp, tLastInputFrameTimestamp, tInputFrameTimestamp - tLastInputFrameTimestamp);
            tLastInputFrameTimestamp = tInputFrameTimestamp;

            mEncoderSeekMutex.lock();

            if ((tBufferSize > 0) && (mEncoderThreadNeeded))
            {
                // lock
                mMediaSinksMutex.lock();

                int tRegisteredMediaSinks = mMediaSinks.size();

                // unlock
                mMediaSinksMutex.unlock();

                //####################################################################
                //### reencode frame and send it to the registered media sinks
                //###################################################################
                if ((mStreamActivated) && (tRegisteredMediaSinks))
                {
                    switch(mMediaType)
                    {
                        case MEDIA_VIDEO:
                            {
                                int64_t tTime3 = Time::GetTimeStamp();
                                // ####################################################################
                                // ### PREPARE YUV FRAME from SCALER
                                // ###################################################################
                                // Assign appropriate parts of buffer to image planes in tRGBFrame
                                avpicture_fill((AVPicture *)tYUVFrame, (uint8_t *)tBuffer, mCodecContext->pix_fmt, mCurrentStreamingResX, mCurrentStreamingResY);

                                #ifdef MSM_DEBUG_TIMING
                                    int64_t tTime5 = Time::GetTimeStamp();
                                    LOG(LOG_VERBOSE, "     preparing data structures took %"PRId64" us", tTime5 - tTime3);
                                #endif

                                tYUVFrame->pict_type = AV_PICTURE_TYPE_NONE;
                                if (!mMediaSource->HasVariableVideoOutputFrameRate())
                                {// base source delivers a stable output frame rate
									tVideoEncoderFrameTimestamp = (int64_t)rint(CalculateEncoderPts(mFrameNumber));
                                }else
                                {// base source delivers a variable output frame rate (we cannot rely on equidistant times between two grabbed frames
                                	if (mEncoderStartTime == 0)
                                	{
                                		LOG(LOG_WARN, "Encoder start time is still invalid, setting a default value");
                                		mEncoderStartTime = tInputFrameTimestamp;
                                	}
                                	tVideoEncoderFrameTimestamp = (tInputFrameTimestamp - mEncoderStartTime) / 1000; // grab time in ms
                                }

                                // check encoder frame timestamp
                                if ((tLastVideoEncoderFrameTimestamp != 0) && (tVideoEncoderFrameTimestamp <= tLastVideoEncoderFrameTimestamp))
                                {// timestamp is too low
									#ifdef MSM_DEBUG_TIMING
                                		LOG(LOG_WARN, "Encoder VIDEO frame timestamp is too low (%"PRId64" <= %"PRId64")", tVideoEncoderFrameTimestamp, tLastVideoEncoderFrameTimestamp);
									#endif

									// enforce a monotonously increasing time base
                                	tVideoEncoderFrameTimestamp = tLastVideoEncoderFrameTimestamp + 1;
                                }

								#ifdef MSM_DEBUG_PACKETS
									LOG(LOG_VERBOSE, "Setting PTS to %lld(last: %lld) for frame %d", tVideoEncoderFrameTimestamp, tVideoEncoderFrameTimestamp, mFrameNumber);
								#endif

                                // store the last timestamp
								tLastVideoEncoderFrameTimestamp = tVideoEncoderFrameTimestamp;

								tYUVFrame->pts = tVideoEncoderFrameTimestamp;
                                tYUVFrame->pkt_pts = tYUVFrame->pts;
                                tYUVFrame->pkt_dts = tYUVFrame->pts;
                                tYUVFrame->coded_picture_number = mFrameNumber;
                                tYUVFrame->coded_picture_number = mFrameNumber;

                                #ifdef MSM_DEBUG_PACKETS
                                    LOG(LOG_VERBOSE, "Distributing VIDEO frame..");
                                    LOG(LOG_VERBOSE, "      ..key frame: %d", tYUVFrame->key_frame);
                                    LOG(LOG_VERBOSE, "      ..frame type: %s-frame", GetFrameType(tYUVFrame).c_str());
                                    LOG(LOG_VERBOSE, "      ..pts: %"PRId64"(scaler pts: %"PRId64")", tYUVFrame->pts, tInputFrameTimestamp);
                                    LOG(LOG_VERBOSE, "      ..pkt_pts: %"PRId64, tYUVFrame->pkt_pts);
                                    LOG(LOG_VERBOSE, "      ..pkt_dts: %"PRId64, tYUVFrame->pkt_dts);
                                    LOG(LOG_VERBOSE, "      ..coded pic number: %d", tYUVFrame->coded_picture_number);
                                    LOG(LOG_VERBOSE, "      ..display pic number: %d", tYUVFrame->display_picture_number);
                                #endif

								// ####################################################################
								// ### calculate output frame timestamp from input frame timestamp
								// ####################################################################
								tOutputFrameTimestamp = tInputFrameTimestamp;

                                // ####################################################################
                                // ### update synch. for all media sinks
                                // ####################################################################
								RelaySyncTimestampToMediaSinks(tOutputFrameTimestamp, tYUVFrame->pts);

                                // ####################################################################
                                // ### generate new output frame
                                // ####################################################################
                                EncodeAndWritePacket(mFormatContext, mCodecContext, tYUVFrame, mEncoderHasKeyFrame, mEncoderBufferedFrames);

                                #ifdef MSM_DEBUG_PACKETS
                                	LOG(LOG_VERBOSE, "Encoder buffered frames: %d, flags: 0x%x", mEncoderBufferedFrames, mCodecContext->codec->capabilities);
								#endif

								// increase the frame counter (used for PTS generation)
                                mFrameNumber++;
                            }
                            break;

                        case MEDIA_AUDIO:
                            {
                                int tOutputAudioBytesPerSample = av_get_bytes_per_sample(mOutputAudioFormat);
                                int tOutputSamplesPerChannel = mCodecContext->frame_size; // nr. of samples of output frames
                                int tReadFifoSize = tOutputSamplesPerChannel * tOutputAudioBytesPerSample * mOutputAudioChannels;
                                int tReadFifoSizePerChannel = tOutputSamplesPerChannel * tOutputAudioBytesPerSample;

                                int tInputAudioBytesPerSample = av_get_bytes_per_sample(mInputAudioFormat);
                                int tInputSamplesPerChannel = tBufferSize / (tInputAudioBytesPerSample * mInputAudioChannels); // nr. of samples of source frames

                                int tResamplingOutputSamples = 0;

                                uint8_t **tInputSamplesPlanes = (uint8_t**)&tBuffer; // we have only one audio input plane because we use PCM S16 for playback everywhere, which is the input here
                                if (mAudioResampleContext != NULL)
                                {// audio resampling needed: we have to insert an intermediate step, which resamples the audio chunk
                                    // ############################
                                    // ### resample input
                                    // ############################
                                    #ifdef MSM_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "Converting %d samples/channel from %p and store it to %p", tInputSamplesPerChannel, *tInputSamplesPlanes, mResampleBuffer);
                                    #endif
                                    //LOG(LOG_VERBOSE, "planes are %p and %p", mResampleBufferPlanes[0], mResampleBufferPlanes[1]);
                                    tResamplingOutputSamples = HM_swr_convert(mAudioResampleContext, &mResampleBufferPlanes[0], MEDIA_SOURCE_SAMPLE_BUFFER_PER_CHANNEL, (const uint8_t**)tInputSamplesPlanes, tInputSamplesPerChannel);
                                    if (tResamplingOutputSamples <= 0)
                                        LOG(LOG_ERROR, "Amount of resampled samples (%d) is invalid", tResamplingOutputSamples);

                                    tInputSamplesPlanes = mResampleBufferPlanes;
                                }

                                int tWrittenFifoSize = tResamplingOutputSamples * tOutputAudioBytesPerSample * mOutputAudioChannels;
                                int tWrittenFifoSizePerChannel = tResamplingOutputSamples * tOutputAudioBytesPerSample;

                                // ####################################################################
                                // ### calculate output frame timestamp from input frame timestamp
                                // ####################################################################
                                // calculate how many samples are already stored within the FIFOs
                                int tAlreadyAvailableSamplesPerChannel = av_fifo_size(mResampleFifo[0]) / tOutputAudioBytesPerSample;
                                // is everything stored within resample FIFO 0?
                                if (!av_sample_fmt_is_planar(mOutputAudioFormat))
                                	tAlreadyAvailableSamplesPerChannel /= mOutputAudioChannels;
                                // calculate the time offset by which we have to shift the input timestamp in order to calculate the output timestamp of the first sample from the FIFOs
                                int64_t tOutputFrameTimestampOffset = 1000 * 1000 * tAlreadyAvailableSamplesPerChannel / GetOutputSampleRate();
								#ifdef MSM_DEBUG_PACKET_DISTRIBUTION
									LOG(LOG_VERBOSE, "Shifting output frame timestamp of %ld by %6ld for %d(FIFO 0: %d) samples and output sample rate of %d Hz", tOutputFrameTimestamp, tOutputFrameTimestampOffset, tAlreadyAvailableSamplesPerChannel, av_fifo_size(mResampleFifo[0]), GetOutputSampleRate());
								#endif
                                tOutputFrameTimestamp = tInputFrameTimestamp - tOutputFrameTimestampOffset;

                                // ####################################################################
                                // ### buffer the input (resampled?) audio date for frame size conversion
                                // ####################################################################
                                for (int i = 0; i < mOutputAudioChannels; i++)
                                {
                                    int tFifoIndex = i;
                                    void *tInputBuffer = (void*)tInputSamplesPlanes[i];
                                    if (!av_sample_fmt_is_planar(mOutputAudioFormat))
                                    {
                                        tInputBuffer = (void*)(tInputSamplesPlanes[0] + i * tWrittenFifoSizePerChannel);
                                        tFifoIndex = 0;
                                    }

                                    // ############################
                                    // ### reallocate FIFO space
                                    // ############################
                                    #ifdef MS_DEBUG_RECORDER_PACKETS
                                        LOG(LOG_VERBOSE, "Adding %d bytes (%d bytes/sample, input channels: %d, frame size: %d) to AUDIO FIFO %d", tWrittenFifoSizePerChannel, tInputAudioBytesPerSample, mInputAudioChannels, mCodecContext->frame_size, tFifoIndex);
                                    #endif
                                    // is there enough space in the FIFO?
                                    if (av_fifo_space(mResampleFifo[tFifoIndex]) < tWrittenFifoSizePerChannel)
                                    {// no, we need reallocation
                                        if (av_fifo_realloc2(mResampleFifo[tFifoIndex], av_fifo_size(mResampleFifo[tFifoIndex]) + tWrittenFifoSizePerChannel - av_fifo_space(mResampleFifo[tFifoIndex])) < 0)
                                        {
                                            // acknowledge failed
                                            LOG(LOG_ERROR, "Reallocation of resample FIFO audio buffer for channel %d failed", i);
                                        }
                                    }
                                    // ############################
                                    // ### write frame content to FIFO
                                    // ############################
                                    #ifdef MSM_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "Writing %d bytes from %u(%u) to FIFO %d", tWrittenFifoSizePerChannel, tInputBuffer, mResampleBufferPlanes[tFifoIndex], tFifoIndex);
                                    #endif
                                    av_fifo_generic_write(mResampleFifo[tFifoIndex], tInputBuffer, tWrittenFifoSizePerChannel, NULL);
                                }

                                // ############################
                                // ### check FIFO for available frames
                                // ############################
                                while (/* planar */((av_sample_fmt_is_planar(mOutputAudioFormat) &&
                                                        (av_fifo_size(mResampleFifo[0]) >= tReadFifoSizePerChannel))) ||
                                      (/* packed/interleaved */((!av_sample_fmt_is_planar(mOutputAudioFormat)) &&
                                                        (av_fifo_size(mResampleFifo[0]) >= tReadFifoSizePerChannel * mOutputAudioChannels /* FIFO 0 stores all samples */))))
                                {
                                    //####################################################################
                                    // create audio planes
                                    // ###################################################################
                                    uint8_t* tOutputBuffer = (uint8_t*)mResampleBuffer;
                                    bool tSilenceAudioFrame = true;
                                    for (int i = 0; i < mOutputAudioChannels; i++)
                                    {
                                        int tFifoIndex = i;
                                        if (!av_sample_fmt_is_planar(mOutputAudioFormat))
                                            tFifoIndex = 0;

                                        // ############################
                                        // ### read buffer from FIFO
                                        // ############################
                                        #ifdef MSM_DEBUG_PACKETS
                                            LOG(LOG_VERBOSE, "Reading %d bytes (%d bytes/sample, frame size: %d samples per packet) from %d bytes of FIFO %d", tReadFifoSizePerChannel, tInputAudioBytesPerSample, mCodecContext->frame_size, av_fifo_size(mResampleFifo[tFifoIndex]), tFifoIndex);
                                        #endif
                                        HM_av_fifo_generic_read(mResampleFifo[tFifoIndex], (void*)tOutputBuffer, tReadFifoSizePerChannel);

                                        if ((!mRelayingSkipAudioSilence) || (!ContainsOnlySilence((void*)tOutputBuffer, tReadFifoSizePerChannel) /* we have to check if the current chunk contains only silence */))
                                        {// okay, we should process this audio frame
                                            tSilenceAudioFrame = false;
                                        }

                                        tOutputBuffer += tReadFifoSizePerChannel;
                                    }

                                    if (!tSilenceAudioFrame)
                                    {
                                        //####################################################################
                                        // create final frame for audio encoder
                                        // ###################################################################
                                        avcodec_get_frame_defaults(tAudioFrame);
                                        // nb_samples
                                        tAudioFrame->nb_samples = tOutputSamplesPerChannel;
                                        // pts
                                        int64_t tCurPts = av_rescale_q(mFrameNumber * mCodecContext->frame_size, (AVRational){1, mOutputAudioSampleRate}, mCodecContext->time_base);
                                        tAudioFrame->pts = tCurPts;
                                        tAudioFrame->pkt_pts = tAudioFrame->pts;
                                        tAudioFrame->pkt_dts = tAudioFrame->pts;

                                         //data
                                        int tRes = 0;
                                        if ((tRes = avcodec_fill_audio_frame(tAudioFrame, mOutputAudioChannels, mOutputAudioFormat, (const uint8_t *)mResampleBuffer, tReadFifoSize, 1)) < 0)
                                            LOG(LOG_ERROR, "Could not fill the audio frame with the provided data from the audio resampling step because of \"%s\"(%d)", strerror(AVUNERROR(tRes)), tRes);
                                        #ifdef MSM_DEBUG_PACKET_DISTRIBUTION
                                            LOG(LOG_VERBOSE, "Distributing sample buffer with PTS: %"PRId64, tCurPts);
                                            LOG(LOG_VERBOSE, "Filling audio frame with buffer size: %d", tReadFifoSize);
                                        #endif

                                        #ifdef MSM_DEBUG_PACKET_DISTRIBUTION
                                            LOG(LOG_VERBOSE, "Distributing AUDIO frame..");
                                            LOG(LOG_VERBOSE, "      ..key frame: %d", tAudioFrame->key_frame);
                                            LOG(LOG_VERBOSE, "      ..frame type: %s-frame", GetFrameType(tAudioFrame).c_str());
                                            LOG(LOG_VERBOSE, "      ..pts: %"PRId64"", tAudioFrame->pts);
                                            LOG(LOG_VERBOSE, "      ..pkt_pts: %"PRId64, tAudioFrame->pkt_pts);
                                            LOG(LOG_VERBOSE, "      ..pkt_dts: %"PRId64, tAudioFrame->pkt_dts);
                                            LOG(LOG_VERBOSE, "      ..coded pic number: %d", tAudioFrame->coded_picture_number);
                                            LOG(LOG_VERBOSE, "      ..display pic number: %d", tAudioFrame->display_picture_number);
                                            LOG(LOG_VERBOSE, "      ..nr. of samples: %d", tAudioFrame->nb_samples);
                                            LOG(LOG_VERBOSE, "Output audio output data planes...");
                                            for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
                                            {
                                                LOG(LOG_VERBOSE, "%d - %p - %d", i, tAudioFrame->data[i], tAudioFrame->linesize[i]);
                                            }
                                        #endif

										// ####################################################################
										// ### update synch. for all media sinks
										// ####################################################################
                                        RelaySyncTimestampToMediaSinks(tOutputFrameTimestamp, tAudioFrame->pts);

                                        // ####################################################################
                                        // ### correct the NTP time by the time the current audio frame would take during playback
                                        // ####################################################################
                                        tOutputFrameTimestampOffset = 1000 * 1000 * tOutputSamplesPerChannel / GetOutputSampleRate();
										#ifdef MSM_DEBUG_PACKET_DISTRIBUTION
                                        	LOG(LOG_VERBOSE, "Shifting output frame timestamp of %ld by %6ld for %d samples and output sample rate of %d Hz", tOutputFrameTimestamp, tOutputFrameTimestampOffset, tOutputSamplesPerChannel, GetOutputSampleRate());
										#endif
										tOutputFrameTimestamp += tOutputFrameTimestampOffset;

                                        // ####################################################################
                                        // ### generate new output frame
                                        // ####################################################################
										EncodeAndWritePacket(mFormatContext, mCodecContext, tAudioFrame, mEncoderHasKeyFrame, mEncoderBufferedFrames);

                                        // increase the frame counter (used for PTS generation)
                                        mFrameNumber++;
                                    }else
                                    {// silence audio frame
                                        mRelayingSkipAudioSilenceSkippedChunks++;
                                        //LOG(LOG_WARN, "Skipping %s data, overall skipped chunks: %"PRId64"", GetMediaTypeStr().c_str(), mRelayingSkipAudioSilenceSkippedChunks);
                                    }
                                }
                                break;
                            }
                        default:
                                LOG(LOG_ERROR, "Media type unknown");
                                break;

                    }
                }else
                    LOG(LOG_VERBOSE, "Skipped %s transcoder task", GetMediaTypeStr().c_str());
            }

            mEncoderSeekMutex.unlock();

            // release FIFO entry lock
            if (tFifoEntry >= 0)
                mEncoderFifo->ReadFifoExclusiveFinished(tFifoEntry);

            // is FIFO near overload situation?
            if (mEncoderFifo->GetUsage() >= MEDIA_SOURCE_MUX_INPUT_QUEUE_SIZE_LIMIT - 4)
            {
                LOG(LOG_WARN, "%s encoder FIFO with %d entries is near overload situation, deleting all stored frames", GetMediaTypeStr().c_str(), mEncoderFifo->GetSize());

                // delete all stored frames: it is a better for the encoding to have a gap instead of frames which have high picture differences
                mEncoderFifo->ClearFifo();
            }
        }else
        {
            LOG(LOG_VERBOSE, "Suspending the transcoder thread for 10 ms");
            Suspend(10 * 1000); // check every 1/100 seconds the state of the FIFO
        }
    }

    LOG(LOG_VERBOSE, "%s encoder left thread main loop", GetMediaTypeStr().c_str());

    mEncoderFifoAvailableMutex.lock();

    LOG(LOG_VERBOSE, "..writing %s codec trailer", GetMediaTypeStr().c_str());

    // write the trailer, if any
    av_write_trailer(mFormatContext);

    switch(mMediaType)
    {
        case MEDIA_VIDEO:
            LOG(LOG_WARN, "VIDEO encoder thread stops scaler thread..");
            tVideoScaler->StopScaler();
            LOG(LOG_VERBOSE, "VIDEO encoder thread stopped scaler thread");

            //HINT: tVideoScaler will be delete as mEncoderFifo

            // Free the YUV frame
            av_free(tYUVFrame);

            break;
        case MEDIA_AUDIO:
            av_free(tAudioFrame);
            break;
        default:
            break;
    }

    av_free(mEncoderChunkBuffer);

    LOG(LOG_VERBOSE, "..closing %s format converter", GetMediaTypeStr().c_str());
    if (!CloseFormatConverter())
    {
        LOG(LOG_ERROR, "Failed to close %s format converter", GetMediaTypeStr().c_str());
    }

    mEncoderFifoState.lock();
    delete mEncoderFifo;
    mEncoderFifo = NULL;
    mEncoderFifoState.unlock();

    mEncoderFifoAvailableMutex.unlock();

    LOG(LOG_WARN, "%s encoder main loop finished for %s media source <<<<<<<<<<<<<<<<", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());

    return NULL;
}

bool MediaSourceMuxer::SupportsDecoderFrameStatistics()
{
    if (mMediaSource != NULL)
        return mMediaSource->SupportsDecoderFrameStatistics();
    else
        return false;
}

int64_t MediaSourceMuxer::DecodedIFrames()
{
    if (mMediaSource != NULL)
        return mMediaSource->DecodedIFrames();
    else
        return mDecodedIFrames;
}

int64_t MediaSourceMuxer::DecodedPFrames()
{
    if (mMediaSource != NULL)
        return mMediaSource->DecodedPFrames();
    else
        return mDecodedPFrames;

}

int64_t MediaSourceMuxer::DecodedBFrames()
{
    if (mMediaSource != NULL)
        return mMediaSource->DecodedBFrames();
    else
        return mDecodedBFrames;
}

int64_t MediaSourceMuxer::DecodedSFrames()
{
    if (mMediaSource != NULL)
        return mMediaSource->DecodedSFrames();
    else
        return mDecodedSFrames;
}

int64_t MediaSourceMuxer::DecodedSIFrames()
{
    if (mMediaSource != NULL)
        return mMediaSource->DecodedSIFrames();
    else
        return mDecodedSIFrames;
}

int64_t MediaSourceMuxer::DecodedSPFrames()
{
    if (mMediaSource != NULL)
        return mMediaSource->DecodedSPFrames();
    else
        return mDecodedSPFrames;
}
#
int64_t MediaSourceMuxer::DecodedBIFrames()
{
    if (mMediaSource != NULL)
        return mMediaSource->DecodedBIFrames();
    else
        return mDecodedBIFrames;
}

int64_t MediaSourceMuxer::GetEndToEndDelay()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetEndToEndDelay();
    else
        return mEndToEndDelay;
}

float MediaSourceMuxer::GetFrameBufferPreBufferingTime()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetFrameBufferPreBufferingTime();
    else
        return mDecoderFramePreBufferTime;
}

void MediaSourceMuxer::SetFrameBufferPreBufferingTime(float pTime)
{
    if (mMediaSource != NULL)
        return mMediaSource->SetFrameBufferPreBufferingTime(pTime);
}

float MediaSourceMuxer::GetFrameBufferTime()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetFrameBufferTime();
    else
        return mDecoderFrameBufferTime;
}

int MediaSourceMuxer::GetFrameBufferCounter()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetFrameBufferCounter();
    else
        return mDecoderFrameBufferTime;
}

int MediaSourceMuxer::GetFrameBufferSize()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetFrameBufferSize();
    else
        return mDecoderFrameBufferTime;
}

void MediaSourceMuxer::SetPreBufferingActivation(bool pActive)
{
    if (mMediaSource != NULL)
        mMediaSource->SetPreBufferingActivation(pActive);
}

void MediaSourceMuxer::SetPreBufferingAutoRestartActivation(bool pActive)
{
    if (mMediaSource != NULL)
        mMediaSource->SetPreBufferingAutoRestartActivation(pActive);
}

int MediaSourceMuxer::GetDecoderOutputFrameDelay()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetDecoderOutputFrameDelay();
    else
        return mDecoderOutputFrameDelay;
}

void MediaSourceMuxer::SetVideoGrabResolution(int pResX, int pResY)
{
    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return;
    }

    if ((pResX != mSourceResX) || (pResY != mSourceResY))
    {
        LOG(LOG_VERBOSE, "Setting video grabbing resolution to %d * %d", pResX, pResY);
        mSourceResX = pResX;
        mSourceResY = pResY;

        int tResX = pResX;
        int tResY = pResY;
        switch(mStreamCodecId)
        {
            case AV_CODEC_ID_H261: // supports QCIF, CIF
                    if (((pResX == 176) && (pResY == 144)) || ((pResX == 352) && (pResY == 288)))
                    {
                        LOG(LOG_VERBOSE, "Resolution %d*%d supported by H.261", pResX, pResY);
                    }else
                    {
                        LOG(LOG_WARN, "Resolution %d*%d unsupported by H.261, will switch to default resolution of 352*288", pResX, pResY);
                        tResX = 352;
                        tResY = 288;
                        break;
                    }
                    if (pResX > 352)
                        tResX = 352;
                    if (pResX < 176)
                        tResX = 176;
                    if (pResY > 288)
                        tResY = 288;
                    if (pResY < 144)
                        tResY = 144;
                    break;
            case AV_CODEC_ID_H263:  // supports QCIF, CIF, CIF4
                    if (((pResX == 128) && (pResY == 96)) || ((pResX == 176) && (pResY == 144)) || ((pResX == 352) && (pResY == 288)) || ((pResX == 704) && (pResY == 576)) || ((pResX == 1408) && (pResY == 1152)))
                    {
                        LOG(LOG_VERBOSE, "Resolution %d*%d supported by H.263", pResX, pResY);
                    }else
                    {
                        LOG(LOG_WARN, "Resolution %d*%d unsupported by H.263, will switch to default resolution of 352*288", pResX, pResY);
                        tResX = 352;
                        tResY = 288;
                        break;
                    }
                    if (pResX > 704)
                        tResX = 704;
                    if (pResX < 176)
                        tResX = 176;
                    if (pResY > 576)
                        tResY = 576;
                    if (pResY < 144)
                        tResY = 144;
                    break;
            case AV_CODEC_ID_H263P:
            default:
                    break;
        }

        if ((tResX != pResX) || (tResY != pResY))
        {
            LOG(LOG_WARN, "Codec %s doesn't support video resolution, changed resolution from %d*%d to %d*%d", GetFormatName(mStreamCodecId).c_str(), pResX, pResY, tResX, tResY);
            pResX = tResX;
            pResY = tResY;
        }

        if (mMediaSourceOpened)
        {
            // lock grabbing
            mGrabMutex.lock();

            CloseMuxer();

            if (mMediaSource != NULL)
              mMediaSource->SetVideoGrabResolution(mSourceResX, mSourceResY);

            OpenVideoMuxer(mSourceResX, mSourceResY, mInputFrameRate);

            // unlock grabbing
            mGrabMutex.unlock();
        }else
        {
            if (mMediaSource != NULL)
            	mMediaSource->SetVideoGrabResolution(mSourceResX, mSourceResY);
        }
    }
}

void MediaSourceMuxer::GetVideoGrabResolution(int &pResX, int &pResY)
{
    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return;
    }

    if (mMediaSource != NULL)
        mMediaSource->GetVideoGrabResolution(pResX, pResY);
}

void MediaSourceMuxer::GetVideoSourceResolution(int &pResX, int &pResY)
{
    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return;
    }

    if (mMediaSource != NULL)
        mMediaSource->GetVideoSourceResolution(pResX, pResY);
}

GrabResolutions MediaSourceMuxer::GetSupportedVideoGrabResolutions()
{
    GrabResolutions tResult;

    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return tResult;
    }

    if (mMediaSource != NULL)
    	tResult = mMediaSource->GetSupportedVideoGrabResolutions();

    return tResult;
}

void MediaSourceMuxer::SetVideoFlipping(bool pHFlip, bool pVFlip)
{
    mVideoHFlip = pHFlip;
    mVideoVFlip = pVFlip;
}

bool MediaSourceMuxer::HasVariableVideoOutputFrameRate()
{
    if (mMediaSource != NULL)
        return mMediaSource->HasVariableVideoOutputFrameRate();
    else
        return false;
}

bool MediaSourceMuxer::IsSeeking()
{
    if (mMediaSource != NULL)
        return mMediaSource->IsSeeking();
    else
        return false;
}

void MediaSourceMuxer::StopGrabbing()
{
    LOG(LOG_VERBOSE, "Going to stop %s-muxer", GetMediaTypeStr().c_str());
    if (mMediaSource != NULL)
    	mMediaSource->StopGrabbing();
    mGrabbingStopped = true;
    LOG(LOG_VERBOSE, "Stopping of %s-muxer completed", GetMediaTypeStr().c_str());
}

bool MediaSourceMuxer::Reset(enum MediaType pMediaType)
{
    bool tResult = false;

    // HINT: closing the grab device resets the media type!
    int tMediaType = (pMediaType == MEDIA_UNKNOWN) ? mMediaType : pMediaType;

    LOG(LOG_VERBOSE, "Going to reset %s muxer", GetMediaTypeStr().c_str());

    //StopGrabbing();

    // lock grabbing
    mGrabMutex.lock();

    CloseMuxer();

    // restart media source, assuming that the last start of the media source was successful
    // otherwise a manual call to Open(Video/Audio)GrabDevice besides this reset function is need
    switch(tMediaType)
    {
        case MEDIA_VIDEO:
            tResult = OpenVideoMuxer(mSourceResX, mSourceResY, mInputFrameRate);
            break;
        case MEDIA_AUDIO:
            tResult = OpenAudioMuxer(mInputAudioSampleRate, mInputAudioChannels);
            break;
        case MEDIA_UNKNOWN:
            LOG(LOG_ERROR, "Media type unknown");
            break;
    }

    // unlock grabbing
    mGrabMutex.unlock();

    return tResult;
}

enum AVCodecID MediaSourceMuxer::GetSourceCodec()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetSourceCodec();
    else
        return AV_CODEC_ID_NONE;
}

string MediaSourceMuxer::GetSourceCodecStr()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetSourceCodecStr();
    else
        return "";
}

string MediaSourceMuxer::GetSourceCodecDescription()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetSourceCodecDescription();
    else
        return "";
}

bool MediaSourceMuxer::SetInputStreamPreferences(std::string pStreamCodec, bool pRtpActivated, bool pDoReset)
{
    if (mMediaSource != NULL)
        return mMediaSource->SetInputStreamPreferences(pStreamCodec, pRtpActivated, pDoReset);
    else
        return 0;
}

int MediaSourceMuxer::GetChunkDropCounter()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetChunkDropCounter();
    else
        return 0;
}

int MediaSourceMuxer::GetChunkBufferCounter()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetFragmentBufferCounter();
    else
        return 0;
}

bool MediaSourceMuxer::StartRecording(std::string pSaveFileName, int pSaveFileQuality, bool pRealTime)
{
    if (mMediaSource != NULL)
    	return mMediaSource->StartRecording(pSaveFileName, pSaveFileQuality, pRealTime);
    else
    	return false;

}

void MediaSourceMuxer::StopRecording()
{
    if (mMediaSource != NULL)
    	mMediaSource->StopRecording();
}

bool MediaSourceMuxer::SupportsRecording()
{
    if (mMediaSource != NULL)
    	return mMediaSource->SupportsRecording();
    else
    	return false;
}

bool MediaSourceMuxer::IsRecording()
{
    if (mMediaSource != NULL)
    	return mMediaSource->IsRecording();
    else
    	return false;
}

int64_t MediaSourceMuxer::RecordingTime()
{
    if (mMediaSource != NULL)
        return mMediaSource->RecordingTime();
    else
        return 0;
}

void MediaSourceMuxer::SetRelayActivation(bool pState)
{
    if (mStreamActivated != pState)
    {
        LOG(LOG_VERBOSE, "Setting relay activation to: %d", pState);
        mStreamActivated = pState;
    }
}

void MediaSourceMuxer::SetRelaySkipSilence(bool pState)
{
    if (mRelayingSkipAudioSilence != pState)
    {
        LOG(LOG_VERBOSE, "Setting \"relay skip silence\" activation to: %d", pState);
        mRelayingSkipAudioSilence = pState;
    }
}

void MediaSourceMuxer::SetRelaySkipSilenceThreshold(int pValue)
{
	if (mAudioSilenceThreshold != pValue)
	{
		LOG(LOG_VERBOSE, "Setting audio silence suppression threshold to: %d", pValue);
		mAudioSilenceThreshold = pValue;
	}
}

int MediaSourceMuxer::GetRelaySkipSilenceThreshold()
{
	return mAudioSilenceThreshold;
}

int MediaSourceMuxer::GetRelaySkipSilenceSkippedChunks()
{
	return mRelayingSkipAudioSilenceSkippedChunks;
}

string MediaSourceMuxer::GetSourceTypeStr()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetSourceTypeStr();
    else
        return "unknown";
}

enum SourceType MediaSourceMuxer::GetSourceType()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetSourceType();
    else
        return SOURCE_UNKNOWN;
}

void MediaSourceMuxer::getVideoDevices(VideoDevices &pVList)
{
    VideoDeviceDescriptor tDevice;
    MediaSources::iterator tIt;

    tDevice.Name = "auto";
    tDevice.Card = "";
    tDevice.Desc = "automatic device selection";

    pVList.push_back(tDevice);

    // lock
    mMediaSourcesMutex.lock();

    for (tIt = mMediaSources.begin(); tIt != mMediaSources.end(); tIt++)
        (*tIt)->getVideoDevices(pVList);

    // unlock
    mMediaSourcesMutex.unlock();
}

void MediaSourceMuxer::getAudioDevices(AudioDevices &pAList)
{
    AudioDeviceDescriptor tDevice;
    MediaSources::iterator tIt;

    tDevice.Name = "auto";
    tDevice.Card = "";
    tDevice.Desc = "automatic device selection";
    tDevice.IoType = "Input/Output";

    pAList.push_back(tDevice);

    // lock
    mMediaSourcesMutex.lock();

    for (tIt = mMediaSources.begin(); tIt != mMediaSources.end(); tIt++)
        (*tIt)->getAudioDevices(pAList);

     // unlock
    mMediaSourcesMutex.unlock();
}

bool MediaSourceMuxer::SelectDevice(std::string pDesiredDevice, enum MediaType pMediaType, bool &pIsNewDevice)
{
    MediaSources::iterator tIt;
    enum MediaType tMediaType = mMediaType;
    // HINT: save the state of the media source because the processing should be
    //       independent from "CloseGrabDevice" which resets this state

    bool tOldMediaSourceOpened = mMediaSourceOpened;
    MediaSource *tOldMediaSource = mMediaSource;
    bool tResult = true;

    pIsNewDevice = false;

    if ((pMediaType == MEDIA_VIDEO) || (pMediaType == MEDIA_AUDIO))
    {
        mMediaType = pMediaType;
        tMediaType = pMediaType;
    }

    LOG(LOG_INFO, "Selecting new device: \"%s\", current %s device: \"%s\"", pDesiredDevice.c_str(), GetMediaTypeStr().c_str(), mCurrentDevice.c_str());
    mDesiredDevice = pDesiredDevice;

    // lock
    mMediaSourcesMutex.lock();

    if (mMediaSources.size() > 0)
    {
        // probe all registered media sources for support of the requested device
		for (tIt = mMediaSources.begin(); tIt != mMediaSources.end(); tIt++)
		{
		    pIsNewDevice = false;
			if ((tResult = (*tIt)->SelectDevice(pDesiredDevice, tMediaType, pIsNewDevice)))
			    break;
		}

		LOG(LOG_VERBOSE, "Probing of all registered media source resulted in a new input source: %d", pIsNewDevice);

		// if we haven't found the right media source yet we check if the selected device is a file
		if ((!tResult) && (pDesiredDevice.size() > 6) && (pDesiredDevice.substr(0, 6) == "FILE: "))
		{
		    // shortly unlock the media sources mutex
		    mMediaSourcesMutex.unlock();

		    string tFileName = pDesiredDevice.substr(6, pDesiredDevice.size() - 6);
		    LOG(LOG_VERBOSE, "Try to open the selected file: %s", tFileName.c_str());
		    MediaSourceFile *tFileSource = new MediaSourceFile(tFileName);
		    RegisterMediaSource(tFileSource);

		    // lock
		    mMediaSourcesMutex.lock();

	        // probe all registered media sources for support for selected file device: we need a correct iterator reference !
	        for (tIt = mMediaSources.begin(); tIt != mMediaSources.end(); tIt++)
	        {
	            pIsNewDevice = false;
	            if ((tResult = (*tIt)->SelectDevice(pDesiredDevice, tMediaType, pIsNewDevice)))
	                break;
	        }
		}

		// do we have a new device selected and does it belong to another MediaSource, then we close the old media source and open the new media source
		if (tResult)
		{
		    if ((mMediaSource != *tIt) || (mCurrentDevice != mDesiredDevice))
		    {
                if (tOldMediaSourceOpened)
                {
                    LOG(LOG_VERBOSE, "Going to close after new device selection");
                    StopGrabbing();

                    // lock grabbing
                    mGrabMutex.lock();

                    CloseGrabDevice();
                }else
                    LOG(LOG_VERBOSE, "Old input source wasn't opened before");

                // switch to new input source
                mMediaSource = *tIt;

                if (tOldMediaSourceOpened)
                {
                    LOG(LOG_VERBOSE, "Going to open after new device selection");
                    switch(tMediaType)
                    {
                        case MEDIA_VIDEO:
                            if (mMediaSource != NULL)
                                mMediaSource->SetVideoGrabResolution(mSourceResX, mSourceResY);
                            if (!OpenVideoGrabDevice(mSourceResX, mSourceResY, mInputFrameRate))
                            {
                                LOG(LOG_WARN, "Failed to open new video media source, selecting old one");
                                mMediaSource = tOldMediaSource;
                                pIsNewDevice = false;
                                while((!(tResult = OpenVideoGrabDevice(mSourceResX, mSourceResY, mInputFrameRate))) && (tIt != mMediaSources.end()))
                                {
                                    LOG(LOG_VERBOSE, "Couldn't open basic video device, will probe next possible basic device");
                                    tIt++;
                                    (*tIt)->SelectDevice(pDesiredDevice, tMediaType, pIsNewDevice);
                                }
                            }
                            break;
                        case MEDIA_AUDIO:
                            if (!OpenAudioGrabDevice(mInputAudioSampleRate, mInputAudioChannels))
                            {
                                LOG(LOG_WARN, "Failed to open new audio media source, selecting old one");
                                mMediaSource = tOldMediaSource;
                                pIsNewDevice = false;
                                while((!(tResult = OpenAudioGrabDevice(mInputAudioSampleRate, mInputAudioChannels))) && (tIt != mMediaSources.end()))
                                {
                                    LOG(LOG_VERBOSE, "Couldn't open basic audio device, will probe next possible basic device");
                                    tIt++;
                                    (*tIt)->SelectDevice(pDesiredDevice, tMediaType, pIsNewDevice);
                                }
                            }
                            break;
                        case MEDIA_UNKNOWN:
                            LOG(LOG_ERROR, "Media type unknown");
                            break;
                    }
                    // unlock grabbing
                    mGrabMutex.unlock();
                }
			}else
			{
			    LOG(LOG_VERBOSE, "Reset of original %s source skipped because it was only re-selected", GetMediaTypeStr().c_str());
			    pIsNewDevice = false;
			}
            mCurrentDevice = mDesiredDevice;
		}else
		    LOG(LOG_WARN, "Couldn't select %s device \"%s\"", GetMediaTypeStr().c_str(), pDesiredDevice.c_str());
    }else
    	LOG(LOG_WARN, "No basic %s source registered until now. Device selection not possible", GetMediaTypeStr().c_str());

    // unlock
    mMediaSourcesMutex.unlock();

    // return true if the process was successful
    return tResult;
}

string MediaSourceMuxer::GetCurrentDeviceName()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetCurrentDeviceName();
    else
        return "";
}

string MediaSourceMuxer::GetCurrentDevicePeerName()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetCurrentDevicePeerName();
    else
        return "";
}

bool MediaSourceMuxer::RegisterMediaSource(MediaSource* pMediaSource)
{
    MediaSources::iterator tIt;
    bool tFound = false;

    LOG(LOG_VERBOSE, "Registering media source: 0x%x", pMediaSource);

    // lock
    mMediaSourcesMutex.lock();

    for (tIt = mMediaSources.begin(); tIt != mMediaSources.end(); tIt++)
    {
        if ((*tIt) == pMediaSource)
        {
            LOG(LOG_VERBOSE, "Source already registered");
            tFound = true;
            break;
        }
    }

    if (!tFound)
        mMediaSources.push_back(pMediaSource);

    if (mMediaSource == NULL)
        mMediaSource = pMediaSource;

    // unlock
    mMediaSourcesMutex.unlock();

    return !tFound;
}

bool MediaSourceMuxer::UnregisterMediaSource(MediaSource* pMediaSource, bool pAutoDelete)
{
    bool tFound = false;
    MediaSources::iterator tIt;

    LOG(LOG_VERBOSE, "Unregistering media source: 0x%x", pMediaSource);

    // lock
    mMediaSourcesMutex.lock();

    for (tIt = mMediaSources.begin(); tIt != mMediaSources.end(); tIt++)
    {
        if ((*tIt) == pMediaSource)
        {
            LOG(LOG_VERBOSE, "Found registered source");
            tFound = true;
            // free memory of media sink object
            if (pAutoDelete)
            {
                LOG(LOG_VERBOSE, "Deleting this media source..");
                delete (*tIt);
            }
            // remove registration of media sink object
            mMediaSources.erase(tIt);
            break;
        }
    }

    if ((tFound) && (mMediaSource == pMediaSource))
    {
        if (mMediaSources.size())
			mMediaSource = *mMediaSources.begin();
        else
        	mMediaSource = NULL;
    }

    // unlock
    mMediaSourcesMutex.unlock();

    return tFound;
}

float MediaSourceMuxer::GetInputFrameRate()
{
    float tResult = -1;

    if (mMediaSource != NULL)
        tResult = mMediaSource->GetInputFrameRate();

    return tResult;
}

void MediaSourceMuxer::SetFrameRate(float pFps)
{
    mInputFrameRate = pFps;
    if (mMediaSource != NULL)
        mMediaSource->SetFrameRate(pFps);
}

int64_t MediaSourceMuxer::GetSynchronizationTimestamp()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetSynchronizationTimestamp();
    else
        return 0;
}

int MediaSourceMuxer::GetSynchronizationPoints()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetSynchronizationPoints();
    else
        return 0;
}

bool MediaSourceMuxer::TimeShift(int64_t pOffset)
{
    bool tResult = false;

    if (mMediaSource != NULL)
    {
        tResult = mMediaSource->TimeShift(pOffset);
        ResetEncoderBuffers();
    }

    return tResult;
}

int MediaSourceMuxer::GetOutputSampleRate()
{
    return mOutputAudioSampleRate;
}

int MediaSourceMuxer::GetOutputChannels()
{
    return mOutputAudioChannels;
}

int MediaSourceMuxer::GetInputSampleRate()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetInputSampleRate();
    else
        return 0;
}

int MediaSourceMuxer::GetInputChannels()
{
    if (mMediaSource != NULL)
    	return mMediaSource->GetInputChannels();
    else
        return 0;
}

string MediaSourceMuxer::GetInputFormatStr()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetInputFormatStr();
    else
        return 0;
}

int MediaSourceMuxer::GetInputBitRate()
{
    if (mMediaSource != NULL)
    	return mMediaSource->GetInputBitRate();
    else
        return 0;
}

void* MediaSourceMuxer::AllocChunkBuffer(int& pChunkBufferSize, enum MediaType pMediaType)
{
    void *tResult = NULL;

    if (mMediaSource != NULL)
    {
    	tResult = mMediaSource->AllocChunkBuffer(pChunkBufferSize, pMediaType);
    }else
    {
    	LOG(LOG_VERBOSE, "%s-muxer has no valid base media source registered, allocating chunk buffer via MediaSource::AllocChunkBuffer", GetMediaTypeStr().c_str());
    	tResult = MediaSource::AllocChunkBuffer(pChunkBufferSize, pMediaType);
    }
    LOG(LOG_VERBOSE, "%s-muxer allocated buffer at %p with size of %d bytes", GetMediaTypeStr().c_str(), tResult, pChunkBufferSize);
    return tResult;
}

void MediaSourceMuxer::FreeChunkBuffer(void *pChunk)
{
    // lock grabbing
    mGrabMutex.lock();

    if (mMediaSource != NULL)
    {
    	mMediaSource->FreeChunkBuffer(pChunk);
	}else
	{
		LOG(LOG_WARN, "%s-muxer has no valid base media source registered, freeing chunk buffer via MediaSource::FreeChunkBuffer", GetMediaTypeStr().c_str());
		MediaSource::FreeChunkBuffer(pChunk);
	}
    // unlock grabbing
    mGrabMutex.unlock();
}

void MediaSourceMuxer::FreeUnusedRegisteredFileSources()
{
    MediaSources::iterator tIt;

    // lock
    mMediaSourcesMutex.lock();

    if (mMediaSources.size() > 0)
    {
        for (tIt = mMediaSources.begin(); tIt != mMediaSources.end(); tIt++)
        {
            if ((*tIt != mMediaSource) && ((*tIt)->GetSourceType() == SOURCE_FILE))
            {
                mMediaSourcesMutex.unlock();
                UnregisterMediaSource(*tIt, true);
                mMediaSourcesMutex.lock();
                tIt = mMediaSources.begin();
            }
        }
    }

    // unlock
    mMediaSourcesMutex.unlock();
}

bool MediaSourceMuxer::SupportsSeeking()
{
    if (mMediaSource != NULL)
        return mMediaSource->SupportsSeeking();
    else
        return false;
}

float MediaSourceMuxer::GetSeekEnd()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetSeekEnd();
    else
        return 0;
}

bool MediaSourceMuxer::Seek(float pSeconds, bool pOnlyKeyFrames)
{
    bool tResult = false;

    if (mMediaSource != NULL)
    {
    	tResult = mMediaSource->Seek(pSeconds, pOnlyKeyFrames);
        ResetEncoderBuffers();
    }

    return tResult;
}

float MediaSourceMuxer::GetSeekPos()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetSeekPos();
    else
        return 0;
}

bool MediaSourceMuxer::SupportsMultipleInputStreams()
{
    if (mMediaSource != NULL)
        return mMediaSource->SupportsMultipleInputStreams();
    else
        return false;
}

bool MediaSourceMuxer::SelectInputStream(int pIndex)
{
    bool tResult = false;

    if (mMediaSource != NULL)
    {
        tResult = mMediaSource->SelectInputStream(pIndex);
        ResetEncoderBuffers();
    }

    return tResult;
}

string MediaSourceMuxer::CurrentInputStream()
{
    if (mMediaSource != NULL)
        return mMediaSource->CurrentInputStream();
    else
        return "";
}

vector<string> MediaSourceMuxer::GetInputStreams()
{
    vector<string> tNone;

    if (mMediaSource != NULL)
        return mMediaSource->GetInputStreams();
    else
        return tNone;
}

bool MediaSourceMuxer::HasInputStreamChanged()
{
    if (mMediaSource != NULL)
        return mMediaSource->HasInputStreamChanged();
    else
        return false;
}

bool MediaSourceMuxer::SupportsMarking()
{
    return true;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
