/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
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
#include <ProcessStatisticService.h>
#include <HBSocket.h>
#include <RTP.h>
#include <Logger.h>

#include <string>
#include <stdint.h>

using namespace std;
using namespace Homer::Monitor;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

MediaSourceMuxer::MediaSourceMuxer(MediaSource *pMediaSource):
    MediaSource("MUX: transcoded capture")
{
    mStreamPacketBuffer = (char*)malloc(MEDIA_SOURCE_MUX_STREAM_PACKET_BUFFER_SIZE);
    mEncoderChunkBuffer = (char*)malloc(MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE);
    mSamplesTempBuffer = (char*)malloc(MEDIA_SOURCE_AUDIO_SAMPLE_BUFFER_SIZE);
    SetOutgoingStream();
    mStreamCodecId = CODEC_ID_NONE;
    mStreamMaxPacketSize = 500;
    mStreamQuality = 20;
    mStreamMaxFps = 0;
    mVideoHFlip = false;
    mVideoVFlip = false;
    mMediaSource = pMediaSource;
    if (mMediaSource != NULL)
    	mMediaSources.push_back(mMediaSource);
    mCurrentStreamingResX = 0;
    mCurrentStreamingResY = 0;
    mRequestedStreamingResX = 352;
    mRequestedStreamingResY = 288;
    mStreamActivated = true;
    mTranscoderNeeded = true;
    mTranscoderHasKeyFrame = false;
    mTranscoderFifo = NULL;
}

MediaSourceMuxer::~MediaSourceMuxer()
{
    if (mMediaSourceOpened)
        mMediaSource->CloseGrabDevice();
    free(mSamplesTempBuffer);
    free(mEncoderChunkBuffer);
    free(mStreamPacketBuffer);

    DeinitTranscoder();
}

///////////////////////////////////////////////////////////////////////////////
// static call back function for ffmpeg encoder
int MediaSourceMuxer::DistributePacket(void *pOpaque, uint8_t *pBuffer, int pBufferSize)
{
    MediaSourceMuxer *tMuxer = (MediaSourceMuxer*)pOpaque;
    char *tBuffer = (char*)pBuffer;

    //####################################################################
    // distribute frame among the registered media sinks
    // ###################################################################
    // no locking of mMediaSinksMutex needed - we are running in context of GrabChunk
    #ifdef MSM_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Distribute packet of size: %d", pBufferSize);
    #endif
    tMuxer->RelayPacketToMediaSinks(tBuffer, (unsigned int)pBufferSize, tMuxer->mTranscoderHasKeyFrame);

    return pBufferSize;
}

///////////////////////////////////////////////////////////////////////////////

bool MediaSourceMuxer::SupportsMuxing()
{
    return true;
}

bool MediaSourceMuxer::SupportsRelaying()
{
    return true;
}

MediaSource* MediaSourceMuxer::GetMediaSource()
{
    return mMediaSource;
}

// return if something has changed
bool MediaSourceMuxer::SetOutputStreamPreferences(std::string pStreamCodec, int pMediaStreamQuality, int pMaxPacketSize, bool pDoReset, int pResX, int pResY, bool pRtpActivated, int pMaxFps)
{
    // HINT: returns if something has changed
    bool tResult = false;
    enum CodecID tStreamCodecId = MediaSource::FfmpegName2FfmpegId(MediaSource::CodecName2FfmpegName(pStreamCodec));

    pMaxPacketSize -= IP6_HEADER_SIZE; // IPv6 overhead is bigger than IPv4
    pMaxPacketSize -= IP_OPTIONS_SIZE; // IP options size: used for QoS signaling
    pMaxPacketSize -= TCP_HEADER_SIZE; // TCP overhead is bigger than UDP/UDPlite
    pMaxPacketSize -= TCP_FRAGMENT_HEADER_SIZE; // TCP fragment header which is used to differentiate the RTP packets (fragments) in a received TCP packet
    pMaxPacketSize -= RTP::GetHeaderSizeMax(tStreamCodecId);
	//pMaxPacketSize -= 32; // additional safety buffer size

    int tOrgResX = pResX;
    int tOrgResY = pResY;

    switch(tStreamCodecId)
    {
            case CODEC_ID_H261: // supports QCIF, CIF
                    if (pResX > 352)
                        pResX = 352;
                    if (pResX < 176)
                        pResX = 176;
                    if (pResY > 288)
                        pResY = 288;
                    if (pResY < 144)
                        pResY = 144;
                    break;
            case CODEC_ID_H263:  // supports QCIF, CIF, CIF4
            case CODEC_ID_H263P:  // supports QCIF, CIF, CIF4
                    if (pResX > 704)
                        pResX = 704;
                    if (pResX < 176)
                        pResX = 176;
                    if (pResY > 576)
                        pResY = 576;
                    if (pResY < 144)
                        pResY = 144;
                    break;
            default:
                    break;
    }

    if ((tOrgResX != pResX) || (tOrgResY != pResY))
    {
        LOG(LOG_WARN, "Codec %s doesn't support the request video resolution of %d * %d, values were replaced by %d * %d", GetCodecName().c_str(), tOrgResX, tOrgResY, pResX, pResY);
    }

    if (mStreamMaxFps != pMaxFps)
    {
        // set new quality
        LOG(LOG_VERBOSE, "    ..stream FPS: %d => %d", mStreamMaxFps, pMaxFps);
        mStreamMaxFps = pMaxFps;
    }

    if ((mStreamCodecId != tStreamCodecId) ||
        (GetRtpActivation() != pRtpActivated) ||
        (mStreamQuality != pMediaStreamQuality) ||
        (mStreamMaxPacketSize != pMaxPacketSize) ||
        (mCurrentStreamingResX != pResX) || (mCurrentStreamingResY != pResY))
    {
        LOG(LOG_VERBOSE, "Setting new %s streaming preferences", GetMediaTypeStr().c_str());

        tResult = true;

        // set new codec
        LOG(LOG_VERBOSE, "    ..stream codec: %d => %d", mStreamCodecId, tStreamCodecId);
        mStreamCodecId = tStreamCodecId;

        // set the stream's maximum packet size
        LOG(LOG_VERBOSE, "    ..stream max packet size: %d => %d", mStreamMaxPacketSize, pMaxPacketSize);
        mStreamMaxPacketSize = pMaxPacketSize;

        // set new quality
        LOG(LOG_VERBOSE, "    ..stream quality: %d => %d", mStreamQuality, pMediaStreamQuality);
        mStreamQuality = pMediaStreamQuality;

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


bool MediaSourceMuxer::OpenVideoMuxer(int pResX, int pResY, float pFps)
{
    int                 tResult;
    ByteIOContext       *tByteIoContext;
    AVOutputFormat      *tFormat;
    AVCodec             *tCodec;
    AVStream            *tStream;

    if (pFps > 29,97)
        pFps = 29,97;
    if (pFps < 5)
        pFps = 5;

    LOG(LOG_VERBOSE, "Going to open muxer, media type is \"%s\"", GetMediaTypeStr().c_str());

    if (mMediaSourceOpened)
        return false;

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, PACKET_TYPE_RAW);

    // build correct IO-context
    tByteIoContext = av_alloc_put_byte((uint8_t*) mStreamPacketBuffer, MEDIA_SOURCE_MUX_STREAM_PACKET_BUFFER_SIZE /*HINT: don't use mStreamMaxPacketSize here */, 1, this, NULL, DistributePacket, NULL);

    // mark as streamed
    tByteIoContext->is_streamed = 1;
    // limit packet size
    tByteIoContext->max_packet_size = mStreamMaxPacketSize;

    mSourceResX = pResX;
    mSourceResY = pResY;
    mFrameRate = pFps;

    // allocate new format context
    mFormatContext = AV_NEW_FORMAT_CONTEXT();

    // find format
    tFormat = AV_GUESS_FORMAT(FfmpegId2FfmpegFormat(mStreamCodecId).c_str(), NULL, NULL);
    if (tFormat == NULL)
    {
        LOG(LOG_ERROR, "Invalid suggested video format");

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    // explicit codec selection for H263, otherwise ffmpeg would use the last H263-selection
    if (mStreamCodecId == CODEC_ID_H263P)
        tFormat->video_codec = CODEC_ID_H263P;
    if (mStreamCodecId == CODEC_ID_H263)
        tFormat->video_codec = CODEC_ID_H263;

    // set correct output format
    mFormatContext->oformat = tFormat;
    // set correct IO-context
    mFormatContext->pb = tByteIoContext;
    // verbose timestamp debugging    mFormatContext->debug = FF_FDEBUG_TS;

    // allocate new stream structure
    tStream = av_new_stream(mFormatContext, 0);
    mCodecContext = tStream->codec;
    mCodecContext->codec_id = tFormat->video_codec;
    mCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    // do some extra modifications for H263
    if (tFormat->video_codec == CODEC_ID_H263P)
        mCodecContext->flags |= CODEC_FLAG_H263P_SLICE_STRUCT | CODEC_FLAG_4MV | CODEC_FLAG_AC_PRED | CODEC_FLAG_H263P_UMV | CODEC_FLAG_H263P_AIV;
    // put sample parameters
    mCodecContext->bit_rate = 90000;
    // resolution
    mCurrentStreamingResX = mRequestedStreamingResX;
    mCodecContext->width = mCurrentStreamingResX;
    mCurrentStreamingResY = mRequestedStreamingResY;
    mCodecContext->height = mCurrentStreamingResY;
    /*
     * time base: this is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. for fixed-FrameRate content,
     * timebase should be 1/framerate and timestamp increments should be
     * identically to 1.
     */
    mCodecContext->time_base.den = (int)mFrameRate * 100;
    mCodecContext->time_base.num = 100;
    tStream->time_base.den = (int)mFrameRate * 100;
    tStream->time_base.num = 100;
    // set i frame distance: GOP = group of pictures
    mCodecContext->gop_size = (100 - mStreamQuality) / 5; // default is 12
    mCodecContext->qmin = 1; // default is 2
    mCodecContext->qmax = 2 +(100 - mStreamQuality) / 4; // default is 31
    // set max. packet size for RTP based packets
    //HINT: don't set if we use H261, otherwise ffmpeg internal functions in mpegvideo_enc.c (MPV_*) would get confused because H261 support is missing for RTP
    if (tFormat->video_codec != CODEC_ID_H261)
        mCodecContext->rtp_payload_size = mStreamMaxPacketSize;
    else
        RTP::SetH261PayloadSizeMax(mStreamMaxPacketSize);

    // workaround for incompatibility of ffmpeg/libx264
    // inspired by check within libx264 in "x264_validate_parameters()" of encoder.c
    if (tFormat->video_codec == CODEC_ID_H264)
    {
        mCodecContext->me_range = 16;
        mCodecContext->max_qdiff = 4;
        mCodecContext->qcompress = 0.6;
    }

    // set MPEG quantizer: for h261/h263/mjpeg use the h263 quantizer, in other cases use the MPEG2 one
//    if ((tFormat->video_codec == CODEC_ID_H261) || (tFormat->video_codec == CODEC_ID_H263) || (tFormat->video_codec == CODEC_ID_H263P) || (tFormat->video_codec == CODEC_ID_MJPEG))
//        mCodecContext->mpeg_quant = 0;
//    else
//        mCodecContext->mpeg_quant = 1;

    // set pixel format
    if (tFormat->video_codec == CODEC_ID_MJPEG)
        mCodecContext->pix_fmt = PIX_FMT_YUVJ420P;
    else
        mCodecContext->pix_fmt = PIX_FMT_YUV420P;

    // activate ffmpeg internal fps emulation
    //mCodecContext->rate_emu = 1;

    // some formats want stream headers to be separate
//    if(mFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
//        mCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

    // reset output stream parameters
    if ((tResult = av_set_parameters(mFormatContext, NULL)) < 0)
    {
        LOG(LOG_ERROR, "Invalid video output format parameters because of \"%s\".", strerror(AVUNERROR(tResult)));
        // free codec and stream 0
        av_freep(&mFormatContext->streams[0]->codec);
        av_freep(&mFormatContext->streams[0]);

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    mMediaStreamIndex = 0;

    // Dump information about device file
    dump_format(mFormatContext, mMediaStreamIndex, "MediaSourceMuxer (video)", true);

    // Find the encoder for the video stream
    if ((tCodec = avcodec_find_encoder(tFormat->video_codec)) == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find a fitting video codec");
        // free codec and stream 0
        av_freep(&mFormatContext->streams[0]->codec);
        av_freep(&mFormatContext->streams[0]);

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    // Open codec
    if ((tResult = avcodec_open(mCodecContext, tCodec)) < 0)
    {
        LOG(LOG_ERROR, "Couldn't open video codec because of \"%s\".", strerror(AVUNERROR(tResult)));
        // free codec and stream 0
        av_freep(&mFormatContext->streams[0]->codec);
        av_freep(&mFormatContext->streams[0]);

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    // allocate software scaler context
    mScalerContext = sws_getContext(mSourceResX, mSourceResY, PIX_FMT_RGB32, mCodecContext->width, mCodecContext->height, mCodecContext->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);

    // init transcoder FIFO based for RGB32 pictures
    InitTranscoder(mSourceResX * mSourceResY * 4 /* bytes per pixel */);

    // allocate streams private data buffer and write the streams header, if any
    av_write_header(mFormatContext);

    //######################################################
    //### give some verbose output
    //######################################################
    mMediaType = MEDIA_VIDEO;
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

bool MediaSourceMuxer::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    bool tResult = false;

    if (pFps > 29.97)
        pFps = 29.97;
    if (pFps < 5)
        pFps = 5;

    // save new fps for later internal calls to OpenVideoGrabDevice of underlaying media source
    mFrameRate = pFps;

    LOG(LOG_VERBOSE, "Going to open muxed video grab device, media type is \"%s\"", GetMediaTypeStr().c_str());

    // set media type early to have verbose debug outputs in case of failures
    mMediaType = MEDIA_VIDEO;

    // first open hardware video source
    if (mMediaSource != NULL)
    {
    	tResult = mMediaSource->OpenVideoGrabDevice(pResX, pResY, pFps);
		if (!tResult)
			return false;
    }

    if (mMediaSourceOpened)
        return false;

    // lock
    mMediaSinksMutex.lock();

    // afterwards open the muxer, independent from the open state of the local video
    if (mMediaSource != NULL)
    	tResult = tResult && OpenVideoMuxer(pResX, pResY, pFps);
    else
    	tResult = OpenVideoMuxer(pResX, pResY, pFps);

    // unlock
    mMediaSinksMutex.unlock();

    return tResult;
}

bool MediaSourceMuxer::OpenAudioMuxer(int pSampleRate, bool pStereo)
{
    int                 tResult;
    ByteIOContext       *tByteIoContext;
    AVOutputFormat      *tFormat;
    AVCodec             *tCodec;
    AVStream            *tStream;

    LOG(LOG_VERBOSE, "Going to open muxer, media type is \"%s\"", GetMediaTypeStr().c_str());

    if (mMediaSourceOpened)
        return false;

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_AUDIO, PACKET_TYPE_RAW);

    // build correct IO-context
    tByteIoContext = av_alloc_put_byte((uint8_t*) mStreamPacketBuffer, MEDIA_SOURCE_MUX_STREAM_PACKET_BUFFER_SIZE /*HINT: don't use mStreamMaxPacketSize here */, 1, this, NULL, DistributePacket, NULL);

    // mark as streamed
    tByteIoContext->is_streamed = 1;
    // limit packet size
    tByteIoContext->max_packet_size = mStreamMaxPacketSize;

    // allocate new format context
    mFormatContext = AV_NEW_FORMAT_CONTEXT();

    // find format
    tFormat = AV_GUESS_FORMAT(FfmpegId2FfmpegFormat(mStreamCodecId).c_str(), NULL, NULL);
    if (tFormat == NULL)
    {
        LOG(LOG_ERROR, "Invalid suggested audio format for codec %d", mStreamCodecId);

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    // set correct output format
    mFormatContext->oformat = tFormat;
    // set correct IO-context
    mFormatContext->pb = tByteIoContext;
    // verbose timestamp debugging    mFormatContext->debug = FF_FDEBUG_TS;

    // allocate new stream structure
    tStream = av_new_stream(mFormatContext, 0);
    mCodecContext = tStream->codec;
    mCodecContext->codec_id = tFormat->audio_codec;
    mCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
    if (mCodecContext->codec_id == CODEC_ID_AMR_NB)
    {
        mCodecContext->channels = 1; // force mono for AMR-NB, TODO: what about input buffer - maybe this is captured in stereo mode?
        mCodecContext->bit_rate = 7950; // force to 7.95kHz , limit is given by libopencore_amrnb
        mSampleRate = 8000; //force 8 kHz for AMR-NB
    }else
    {
        mCodecContext->channels = pStereo?2:1; // stereo?
        mCodecContext->bit_rate = MediaSource::AudioQuality2BitRate(mStreamQuality); // streaming rate
        mSampleRate = pSampleRate;
    }
    mStereo = pStereo;
    mCodecContext->sample_rate = mSampleRate; // sampling rate: 22050, 44100
    mCodecContext->qmin = 2; // 2
    mCodecContext->qmax = 9;/*2 +(100 - mAudioStreamQuality) / 4; // 31*/
    mCodecContext->sample_fmt = SAMPLE_FMT_S16;
    // set max. packet size for RTP based packets
    mCodecContext->rtp_payload_size = mStreamMaxPacketSize;

    // some formats want stream headers to be separate
//    if(mFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
//        mCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

    // reset output stream parameters
    if ((tResult = av_set_parameters(mFormatContext, NULL)) < 0)
    {
        LOG(LOG_ERROR, "Invalid audio output format parameters because of \"%s\".", strerror(AVUNERROR(tResult)));
        // free codec and stream 0
        av_freep(&mFormatContext->streams[0]->codec);
        av_freep(&mFormatContext->streams[0]);

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    mMediaStreamIndex = 0;

    // Dump information about device file
    dump_format(mFormatContext, mMediaStreamIndex, "MediaSourceMuxer (audio)", true);

    // Find the encoder for the audio stream
    if((tCodec = avcodec_find_encoder(tFormat->audio_codec)) == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find a fitting audio codec");
        // free codec and stream 0
        av_freep(&mFormatContext->streams[0]->codec);
        av_freep(&mFormatContext->streams[0]);

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    // Inform the codec that we can handle truncated bitstreams -- i.e.,
    // bitstreams where sample boundaries can fall in the middle of packets
//    if(tCodec->capabilities & CODEC_CAP_TRUNCATED)
//        mCodecContext->flags |= CODEC_FLAG_TRUNCATED;

    // Open codec
    if ((tResult = avcodec_open(mCodecContext, tCodec)) < 0)
    {
        LOG(LOG_ERROR, "Couldn't open audio codec because of \"%s\".", strerror(AVUNERROR(tResult)));
        // free codec and stream 0
        av_freep(&mFormatContext->streams[0]->codec);
        av_freep(&mFormatContext->streams[0]);

        // Close the format context
        av_free(mFormatContext);

        return false;
    }

    // avoid packets with frame size of 1 in case of PCM codec
    if (mCodecContext->frame_size == 1)
        mCodecContext->frame_size = 256;

    // init transcoder FIFO based for 2048 samples with 16 bit and 2 channels, more samples are never produced by a media source per grabbing cycle
    InitTranscoder(8192);

    // allocate streams private data buffer and write the streams header, if any
    av_write_header(mFormatContext);

    // init fifo buffer
    mSampleFifo = HM_av_fifo_alloc(MEDIA_SOURCE_AUDIO_SAMPLE_BUFFER_SIZE * 2);

    mMediaType = MEDIA_AUDIO;
    MarkOpenGrabDeviceSuccessful();
    LOG(LOG_INFO, "    ..max packet size: %d bytes", mFormatContext->pb->max_packet_size);
    LOG(LOG_INFO, "  stream...");
    if (mRtpActivated)
        LOG(LOG_INFO, "    ..rtp encapsulation: yes");
    else
        LOG(LOG_INFO, "    ..rtp encapsulation: no");
    LOG(LOG_INFO, "    ..max. packet size: %d bytes", mStreamMaxPacketSize);
    LOG(LOG_INFO, "Fifo opened...");
    LOG(LOG_INFO, "    ..fill size: %d bytes", av_fifo_size(mSampleFifo));

    return true;
}


bool MediaSourceMuxer::OpenAudioGrabDevice(int pSampleRate, bool pStereo)
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to open muxed audio grab device, media type is \"%s\"", GetMediaTypeStr().c_str());

    // set media type early to have verbose debug outputs in case of failures
    mMediaType = MEDIA_AUDIO;

    // first open hardware video source
    if (mMediaSource != NULL)
    {
		tResult = mMediaSource->OpenAudioGrabDevice(pSampleRate, pStereo);
		if (!tResult)
			return false;
    }

    if (mMediaSourceOpened)
        return false;

    // lock
    mMediaSinksMutex.lock();

    // afterwards open the muxer, independent from the open state of the local video
    if (mMediaSource != NULL)
    	tResult = tResult && OpenAudioMuxer(pSampleRate, pStereo);
    else
    	tResult = OpenAudioMuxer(pSampleRate, pStereo);

    // unlock
    mMediaSinksMutex.unlock();

    return tResult;
}

bool MediaSourceMuxer::CloseMuxer()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close muxer, media type is \"%s\"", GetMediaTypeStr().c_str());

    if (mMediaSourceOpened)
    {
        mMediaSourceOpened = false;

        // write the trailer, if any
        av_write_trailer(mFormatContext);

        switch(mMediaType)
        {
            case MEDIA_VIDEO:
                    // free the software scaler context
                    sws_freeContext(mScalerContext);

                    break;
            case MEDIA_AUDIO:
                    // free fifo buffer
                    av_fifo_free(mSampleFifo);

                    break;
            case MEDIA_UNKNOWN:
                    LOG(LOG_ERROR, "Media type unknown");
                    break;
        }

        // Close the codec
        avcodec_close(mCodecContext);

        // free codec and stream 0
        av_freep(&mFormatContext->streams[0]->codec);
        av_freep(&mFormatContext->streams[0]);

        // Close the format context
        av_free(mFormatContext);

        // free the memory of the transcoder FIFO
        DeinitTranscoder();

        LOG(LOG_INFO, "...closed, media type is \"%s\"", GetMediaTypeStr().c_str());

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open, media type is \"%s\"", GetMediaTypeStr().c_str());

    mMediaType = MEDIA_UNKNOWN;

    ResetPacketStatistic();

    return tResult;

}

bool MediaSourceMuxer::CloseGrabDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close, media type is \"%s\"", GetMediaTypeStr().c_str());

    if (mMediaSourceOpened)
    {
        // lock
        mMediaSinksMutex.lock();

        CloseMuxer();

        // unlock
        mMediaSinksMutex.unlock();

        tResult = true;
    }else
    	LOG(LOG_INFO, "The muxer was never opened, media type is \"%s\"", GetMediaTypeStr().c_str());

    if (mMediaSource != NULL)
    	tResult = mMediaSource->CloseGrabDevice() && tResult;
    else
    	LOG(LOG_INFO, "No media source available, media type is \"%s\"", GetMediaTypeStr().c_str());

    mGrabbingStopped = false;

    return tResult;
}

int MediaSourceMuxer::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    MediaSinksList::iterator     tIt;
    int                         tResult;
    AVFrame                     *tRGBFrame;
    AVFrame                     *tYUVFrame;
    AVPacket                    tPacket;
    int                         tFrameSize;

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

    //####################################################################
    // get frame from the original media source
    // ###################################################################
    tResult = mMediaSource->GrabChunk(pChunkBuffer, pChunkSize, pDropChunk);
    #ifdef MSM_DEBUG_PACKETS
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
        MarkGrabChunkFailed(GetMediaTypeStr() + "muxer is closed");

        return tResult;
    }

    // lock
    mMediaSinksMutex.lock();

    //####################################################################
    // reencode frame and send it to the registered media sinks
    // limit the outgoing stream FPS to the defined maximum FPS value
    // ###################################################################
    if ((mStreamActivated) && (!pDropChunk) && (tResult >= 0) && (pChunkSize > 0) && (mMediaSinks.size()) && (BelowMaxFps(tResult)))
    {
        mTranscoderFifo->WriteFifo((char*)pChunkBuffer, pChunkSize);
    }

    // unlock
    mMediaSinksMutex.unlock();

    // unlock grabbing
    mGrabMutex.unlock();

    // acknowledge success
    MarkGrabChunkSuccessful();

    return tResult;
}

bool MediaSourceMuxer::BelowMaxFps(int pFrameNumber)
{
    int64_t tCurrentTime = Time::GetTimeStamp();
    int64_t tTimeDiff = tCurrentTime - mStreamMaxFpsTimestampLastFragment;

    if (mStreamMaxFps != 0)
    {
        int64_t tTimeDiffTreshold = 1000*1000 / mStreamMaxFps;

        #ifdef MSM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Checking max. FPS(=%d) for frame number %d: %lld < %lld?", mStreamMaxFps, pFrameNumber, tTimeDiff, tTimeDiffTreshold);
        #endif

        //### skip capturing when we are too slow
        if (tTimeDiff < tTimeDiffTreshold)
        {
            //LOG(LOG_VERBOSE, "Max. FPS reached, dropping frame %d", pFrameNumber);

            return false;
        }
    }

    if(mStreamMaxFpsChunkNumberLastFragment != pFrameNumber)
    {
    	mStreamMaxFpsTimestampLastFragment = tCurrentTime;
    	mStreamMaxFpsChunkNumberLastFragment = pFrameNumber;
    }

    return true;
}

void MediaSourceMuxer::InitTranscoder(int pFifoEntrySize)
{
    LOG(LOG_VERBOSE, "Init transcoder with FIFO entry size of %d bytes", pFifoEntrySize);

    if (mTranscoderFifo != NULL)
    {
        LOG(LOG_ERROR, "FIFO already initiated");
        delete mTranscoderFifo;
    }else
    {
        mTranscoderFifo = new MediaFifo(MEDIA_SOURCE_MUX_INPUT_QUEUE_SIZE_LIMIT, pFifoEntrySize);

        mTranscoderNeeded = true;

        // start transcoder main loop
        StartThread();
    }
}

void MediaSourceMuxer::DeinitTranscoder()
{
    char tTmp[4];

    LOG(LOG_VERBOSE, "Deinit transcoder");

    if (mTranscoderFifo != NULL)
    {
        // tell transcoder thread it isn't needed anymore
        mTranscoderNeeded = false;

        // write fake data to awake transcoder thread in every case
        mTranscoderFifo->WriteFifo(tTmp, 0);

        // wait for termination of transcoder thread
        if (StopThread())
        {
            delete mTranscoderFifo;
            mTranscoderFifo = NULL;
        }
    }
}

void* MediaSourceMuxer::Run(void* pArgs)
{
    char *tBuffer;
    int tBufferSize;
    int tFifoEntry = 0;

    int                         tResult;
    AVFrame                     *tRGBFrame;
    AVFrame                     *tYUVFrame;
    AVPacket                    tPacket;
    int                         tFrameSize;

    LOG(LOG_VERBOSE, "Transcoder started, media type is \"%s\"", GetMediaTypeStr().c_str());
    switch(mMediaType)
    {
        case MEDIA_VIDEO:
            SVC_PROCESS_STATISTIC.AssignThreadName("Video-Transcoder(" + FfmpegId2FfmpegFormat(mStreamCodecId) + ")");
            break;
        case MEDIA_AUDIO:
            SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Transcoder(" + FfmpegId2FfmpegFormat(mStreamCodecId) + ")");
            break;
        default:
            LOG(LOG_ERROR, "Unknown media type");
            break;
    }

    while(mTranscoderNeeded)
    {
        if (mTranscoderFifo != NULL)
        {
            tFifoEntry = mTranscoderFifo->ReadFifoExclusive(&tBuffer, tBufferSize);

            if (tBufferSize > 0)
            {
                // lock
                mMediaSinksMutex.lock();

                //####################################################################
                // reencode frame and send it to the registered media sinks
                // ###################################################################
                if ((mStreamActivated) && (mMediaSinks.size()))
                {
                    switch(mMediaType)
                    {
                        case MEDIA_VIDEO:
                                //####################################################################
                                // re-encode the frame
                                // ###################################################################
                                // Allocate video frame structure for RGB and YUV format, afterwards allocate data buffer for YUV format
                                if (((tRGBFrame = avcodec_alloc_frame()) == NULL) || ((tYUVFrame = avcodec_alloc_frame()) == NULL) || (avpicture_alloc((AVPicture*)tYUVFrame, PIX_FMT_YUV420P, mCurrentStreamingResX, mCurrentStreamingResY) < 0))
                                {
                                    LOG(LOG_ERROR, "Couldn't allocate video frame memory");
                                }else
                                {
                                    // Assign appropriate parts of buffer to image planes in tRGBFrame
                                    avpicture_fill((AVPicture *)tRGBFrame, (uint8_t *)tBuffer, PIX_FMT_RGB32, mSourceResX, mSourceResY);

                                    // set frame number in corresponding entries within AVFrame structure
                                    tRGBFrame->pts = mChunkNumber + 1;
                                    tRGBFrame->coded_picture_number = mChunkNumber + 1;
                                    tRGBFrame->display_picture_number = mChunkNumber + 1;

                                    #ifdef MSM_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "Reencoding video frame..");
                                        LOG(LOG_VERBOSE, "      ..key frame: %d", tRGBFrame->key_frame);
                                        switch(tRGBFrame->pict_type)
                                        {
                                                case FF_I_TYPE:
                                                    LOG(LOG_VERBOSE, "      ..picture type: i-frame");
                                                    break;
                                                case FF_P_TYPE:
                                                    LOG(LOG_VERBOSE, "      ..picture type: p-frame");
                                                    break;
                                                case FF_B_TYPE:
                                                    LOG(LOG_VERBOSE, "      ..picture type: b-frame");
                                                    break;
                                                default:
                                                    LOG(LOG_VERBOSE, "      ..picture type: %d", tRGBFrame->pict_type);
                                                    break;
                                        }
                                        LOG(LOG_VERBOSE, "      ..pts: %ld", tRGBFrame->pts);
                                        LOG(LOG_VERBOSE, "      ..coded pic number: %d", tRGBFrame->coded_picture_number);
                                        LOG(LOG_VERBOSE, "      ..display pic number: %d", tRGBFrame->display_picture_number);
                                    #endif

                                    // convert fromn RGB to YUV420
                                    HM_sws_scale(mScalerContext, tRGBFrame->data, tRGBFrame->linesize, 0, mSourceResY, tYUVFrame->data, tYUVFrame->linesize);

                                    // #########################################
                                    // re-encode the frame
                                    // #########################################
                                    tFrameSize = avcodec_encode_video(mCodecContext, (uint8_t *)mEncoderChunkBuffer, MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE, tYUVFrame);
                                    mTranscoderHasKeyFrame = (tYUVFrame->pict_type == FF_I_TYPE);

                                    if (tFrameSize > 0)
                                    {
                                        av_init_packet(&tPacket);
                                        mChunkNumber++;

                                        // adapt pts value
                                        if ((mCodecContext->coded_frame) && (mCodecContext->coded_frame->pts != 0))
                                        {
                                            tPacket.pts = av_rescale_q(mCodecContext->coded_frame->pts, mCodecContext->time_base, mFormatContext->streams[mMediaStreamIndex]->time_base);
                                            tPacket.dts = tPacket.pts;
                                        }
                                        // mark i-frame
                                        if (mCodecContext->coded_frame->key_frame)
                                            tPacket.flags |= AV_PKT_FLAG_KEY;

                                        // we only have one stream per video stream
                                        tPacket.stream_index = 0;
                                        tPacket.data = (uint8_t *)mEncoderChunkBuffer;
                                        tPacket.size = tFrameSize;
                                        tPacket.pts = mChunkNumber;
                                        tPacket.dts = mChunkNumber;
                                        //tPacket.pos = av_gettime() - mStartPts;

                                        #ifdef MSM_DEBUG_PACKETS
                                            LOG(LOG_VERBOSE, "Sending video packet: %5d to %2d sink(s):", mChunkNumber, mMediaSinks.size());
                                            LOG(LOG_VERBOSE, "      ..duration: %d", tPacket.duration);
                                            LOG(LOG_VERBOSE, "      ..pts: %ld stream [%d] pts: %ld", tPacket.pts, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->pts);
                                            LOG(LOG_VERBOSE, "      ..dts: %ld", tPacket.dts);
                                            LOG(LOG_VERBOSE, "      ..size: %d", tPacket.size);
                                            LOG(LOG_VERBOSE, "      ..pos: %ld", tPacket.pos);
                                        #endif

                                        // log statistics
                                        AnnouncePacket(tPacket.size);

                                        //####################################################################
                                        // distribute the encoded frame
                                        // ###################################################################
                                        if (av_write_frame(mFormatContext, &tPacket) != 0)
                                         {
                                             LOG(LOG_ERROR, "Couldn't distribute video frame among registered video sinks");
                                         }
                                    }else
                                        LOG(LOG_INFO, "Couldn't re-encode current video frame");

                                    // Free the RGB frame
                                    av_free(tRGBFrame);

                                    // Free the YUV frame's data buffer
                                    avpicture_free((AVPicture*)tYUVFrame);

                                    // Free the YUV frame
                                    av_free(tYUVFrame);
                                }

                                break;

                        case MEDIA_AUDIO:
                                // increase fifo buffer size by size of input buffer size
                                #ifdef MSM_DEBUG_PACKETS
                                    LOG(LOG_VERBOSE, "Adding %d bytes to fifo buffer with size of %d bytes", tBufferSize, av_fifo_size(mSampleFifo));
                                #endif
                                if (av_fifo_realloc2(mSampleFifo, av_fifo_size(mSampleFifo) + tBufferSize) < 0)
                                {
                                    // unlock grabbing
                                    mGrabMutex.unlock();

                                    // acknowledge failed
                                    MarkGrabChunkFailed("reallocation of FIFO audio buffer failed");

                                    break;
                                }

                                //LOG(LOG_VERBOSE, "ChunkSize: %d", pChunkSize);
                                // write new samples into fifo buffer
                                av_fifo_generic_write(mSampleFifo, tBuffer, tBufferSize, NULL);
                                //LOG(LOG_VERBOSE, "ChunkSize: %d", pChunkSize);

                                while (av_fifo_size(mSampleFifo) >= 2 * mCodecContext->frame_size * mCodecContext->channels)
                                {
                                    #ifdef MSM_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "Reading %d bytes from %d bytes of fifo", 2 * mCodecContext->frame_size * mCodecContext->channels, av_fifo_size(mSampleFifo));
                                    #endif
                                    // read sample data from the fifo buffer
                                    HM_av_fifo_generic_read(mSampleFifo, (void*)mSamplesTempBuffer, /* assume signed 16 bit */ 2 * mCodecContext->frame_size * mCodecContext->channels);

                                    //####################################################################
                                    // re-encode the sample
                                    // ###################################################################
                                    // re-encode the sample
                                    #ifdef MSM_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "Reencoding audio frame..");
                                    #endif
                                    //printf("Gonna encode with frame_size %d and channels %d\n", mCodecContext->frame_size, mCodecContext->channels);
                                    int tEncodingResult = avcodec_encode_audio(mCodecContext, (uint8_t *)mEncoderChunkBuffer, /* assume signed 16 bit */ 2 * mCodecContext->frame_size * mCodecContext->channels, (const short *)mSamplesTempBuffer);
                                    mTranscoderHasKeyFrame = true;

                                    //printf("encoded to mp3: %d\n\n", tSampleSize);
                                    if (tEncodingResult > 0)
                                    {
                                        av_init_packet(&tPacket);
                                        mChunkNumber++;

                                        // adapt pts value
                                        if ((mCodecContext->coded_frame) && (mCodecContext->coded_frame->pts != 0))
                                            tPacket.pts = av_rescale_q(mCodecContext->coded_frame->pts, mCodecContext->time_base, mFormatContext->streams[mMediaStreamIndex]->time_base);
                                        tPacket.flags |= AV_PKT_FLAG_KEY;

                                        // we only have one stream per audio stream
                                        tPacket.stream_index = 0;
                                        tPacket.data = (uint8_t *)mEncoderChunkBuffer;
                                        tPacket.size = tEncodingResult;
                                        tPacket.pts = mChunkNumber;
                                        tPacket.dts = mChunkNumber;
            //                            tPacket.pos = av_gettime() - mStartPts;

                                        #ifdef MSM_DEBUG_PACKETS
                                            LOG(LOG_VERBOSE, "Sending audio packet: %5d to %2d sink(s):", mChunkNumber, mMediaSinks.size());
                                            LOG(LOG_VERBOSE, "      ..pts: %ld", tPacket.pts);
                                            LOG(LOG_VERBOSE, "      ..dts: %ld", tPacket.dts);
                                            LOG(LOG_VERBOSE, "      ..size: %d", tPacket.size);
                                            LOG(LOG_VERBOSE, "      ..pos: %ld", tPacket.pos);
                                        #endif

                                        // log statistics
                                        AnnouncePacket(tPacket.size);

                                        //####################################################################
                                        // distribute the encoded frame
                                        // ###################################################################
                                         if (av_write_frame(mFormatContext, &tPacket) != 0)
                                         {
                                             LOG(LOG_ERROR, "Couldn't distribute audio sample among registered audio sinks");
                                         }
                                    }else
                                        LOG(LOG_INFO, "Couldn't re-encode current audio sample");
                                }
                                break;

                        default:
                                LOG(LOG_ERROR, "Media type unknown");
                                break;

                    }
                }

                // unlock
                mMediaSinksMutex.unlock();
            }

            // release FIFO entry lock
            mTranscoderFifo->ReadFifoExclusiveFinished(tFifoEntry);
        }else
            Suspend(100 * 1000); // check every 1/10 seconds the state of the FIFO
    }

    return NULL;
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
        mSourceResX = pResX;
        mSourceResY = pResY;

        if (mMediaSourceOpened)
        {
            // lock grabbing
            mGrabMutex.lock();

            // lock
            mMediaSinksMutex.lock();

            CloseMuxer();

            // unlock
            mMediaSinksMutex.unlock();

            if (mMediaSource != NULL)
              mMediaSource->SetVideoGrabResolution(mSourceResX, mSourceResY);

            // lock
            mMediaSinksMutex.lock();

            OpenVideoMuxer(mSourceResX, mSourceResY, mFrameRate);

            // unlock
            mMediaSinksMutex.unlock();

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

void MediaSourceMuxer::StopGrabbing()
{
    LOG(LOG_VERBOSE, "Going to stop muxer, media type is \"%s\"", GetMediaTypeStr().c_str());
    if (mMediaSource != NULL)
    	mMediaSource->StopGrabbing();
    mGrabbingStopped = true;
    LOG(LOG_VERBOSE, "Stopping of muxer completed");
}

string MediaSourceMuxer::GetCodecName()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetCodecName();
    else
        return "";
}

string MediaSourceMuxer::GetCodecLongName()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetCodecLongName();
    else
        return "";
}

int MediaSourceMuxer::GetChunkDropConter()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetChunkDropConter();
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

void MediaSourceMuxer::SetActivation(bool pState)
{
    mStreamActivated = pState;
}

void MediaSourceMuxer::getVideoDevices(VideoDevicesList &pVList)
{
    VideoDeviceDescriptor tDevice;
    MediaSourcesList::iterator tIt;

    // lock
    mMediaSourcesMutex.lock();

    for (tIt = mMediaSources.begin(); tIt != mMediaSources.end(); tIt++)
        (*tIt)->getVideoDevices(pVList);

    tDevice.Name = "auto";
    tDevice.Card = "";
    tDevice.Desc = "automatic device selection";

    pVList.push_front(tDevice);

    // unlock
    mMediaSourcesMutex.unlock();
}

void MediaSourceMuxer::getAudioDevices(AudioDevicesList &pAList)
{
    AudioDeviceDescriptor tDevice;
    MediaSourcesList::iterator tIt;

    // lock
    mMediaSourcesMutex.lock();

    for (tIt = mMediaSources.begin(); tIt != mMediaSources.end(); tIt++)
        (*tIt)->getAudioDevices(pAList);

    tDevice.Name = "auto";
    tDevice.Card = "";
    tDevice.Desc = "automatic device selection";
    tDevice.IoType = "Input/Output";

    pAList.push_front(tDevice);

     // unlock
    mMediaSourcesMutex.unlock();
}

bool MediaSourceMuxer::SelectDevice(std::string pDesiredDevice, enum MediaType pMediaType, bool &pIsNewDevice)
{
    MediaSourcesList::iterator tIt;
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

    LOG(LOG_INFO, "Selecting new device: \"%s\", current device: \"%s\", media type is \"%s\"", pDesiredDevice.c_str(), mCurrentDevice.c_str(), GetMediaTypeStr().c_str());
    mDesiredDevice = pDesiredDevice;

    // lock
    mMediaSourcesMutex.lock();

    if (mMediaSources.size() > 0)
    {
        // probe all registered media sources for support for selected device
		for (tIt = mMediaSources.begin(); tIt != mMediaSources.end(); tIt++)
		{
		    pIsNewDevice = false;
			if ((tResult = (*tIt)->SelectDevice(pDesiredDevice, tMediaType, pIsNewDevice)))
			    break;
		}

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

	        // probe all registered media sources for support for selected file device
	        for (tIt = mMediaSources.begin(); tIt != mMediaSources.end(); tIt++)
	        {
	            pIsNewDevice = false;
	            if ((tResult = (*tIt)->SelectDevice(pDesiredDevice, tMediaType, pIsNewDevice)))
	                break;
	        }
		}

		// do we have a new device selected and does it come from another MediaSource?
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
                }
                mMediaSource = *tIt;
                if (tOldMediaSourceOpened)
                {
                    LOG(LOG_VERBOSE, "Going to open after new device selection");
                    switch(tMediaType)
                    {
                        case MEDIA_VIDEO:
                            if (mMediaSource != NULL)
                                mMediaSource->SetVideoGrabResolution(mSourceResX, mSourceResY);
                            if (!OpenVideoGrabDevice(mSourceResX, mSourceResY, mFrameRate))
                            {
                                LOG(LOG_WARN, "Failed to open new video media source, selecting old one");
                                mMediaSource = tOldMediaSource;
                                pIsNewDevice = false;
                                tResult = false;
                                OpenVideoGrabDevice(mSourceResX, mSourceResY, mFrameRate);
                            }
                            break;
                        case MEDIA_AUDIO:
                            if (!OpenAudioGrabDevice(mSampleRate, mStereo))
                            {
                                LOG(LOG_WARN, "Failed to open new audio media source, selecting old one");
                                mMediaSource = tOldMediaSource;
                                pIsNewDevice = false;
                                tResult = false;
                                OpenAudioGrabDevice(mSampleRate, mStereo);
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
			    LOG(LOG_VERBOSE, "Reset of original media source skipped because it was only re-selected, media type is \"%s\"", GetMediaTypeStr().c_str());
			    pIsNewDevice = false;
			}
            mCurrentDevice = mDesiredDevice;
		}else
		    LOG(LOG_WARN, "Couldn't select device \"%s\", media type is \"%s\"", pDesiredDevice.c_str(), GetMediaTypeStr().c_str());
    }else
    	LOG(LOG_ERROR, "No basic media source registered until now. Device selection is impossible, media type is \"%s\"", GetMediaTypeStr().c_str());

    // unlock
    mMediaSourcesMutex.unlock();

    // return true if a reset of the new selected device is needed
    return tResult;
}

string MediaSourceMuxer::GetCurrentDeviceName()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetCurrentDeviceName();
    else
        return "";
}

bool MediaSourceMuxer::RegisterMediaSource(MediaSource* pMediaSource)
{
    MediaSourcesList::iterator tIt;
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
    MediaSourcesList::iterator tIt;

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
                delete (*tIt);
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

float MediaSourceMuxer::GetFrameRate()
{
    float tResult = 15;

    if (mMediaSource != NULL)
        tResult = mMediaSource->GetFrameRate();

    return tResult;
}

void MediaSourceMuxer::SetFrameRate(float pFps)
{
    mFrameRate = pFps;
    if (mMediaSource != NULL)
        mMediaSource->SetFrameRate(pFps);
}

void* MediaSourceMuxer::AllocChunkBuffer(int& pChunkBufferSize, enum MediaType pMediaType)
{
    void *tResult = NULL;

    if (mMediaSource != NULL)
    	tResult = mMediaSource->AllocChunkBuffer(pChunkBufferSize, pMediaType);

    return tResult;
}

void MediaSourceMuxer::FreeChunkBuffer(void *pChunk)
{
    // lock grabbing
    mGrabMutex.lock();

    if (mMediaSource != NULL)
    	mMediaSource->FreeChunkBuffer(pChunk);

    // unlock grabbing
    mGrabMutex.unlock();
}

bool MediaSourceMuxer::SupportsSeeking()
{
    if (mMediaSource != NULL)
        return mMediaSource->SupportsSeeking();
    else
        return false;
}

int64_t MediaSourceMuxer::GetSeekEnd()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetSeekEnd();
    else
        return 0;
}

bool MediaSourceMuxer::Seek(int64_t pSeconds, bool pOnlyKeyFrames)
{
    if (mMediaSource != NULL)
        return mMediaSource->Seek(pSeconds, pOnlyKeyFrames);
    else
        return false;
}

bool MediaSourceMuxer::SeekRelative(int64_t pSeconds, bool pOnlyKeyFrames)
{
    if (mMediaSource != NULL)
        return mMediaSource->SeekRelative(pSeconds, pOnlyKeyFrames);
    else
        return false;
}

int64_t MediaSourceMuxer::GetSeekPos()
{
    if (mMediaSource != NULL)
        return mMediaSource->GetSeekPos();
    else
        return 0;
}

bool MediaSourceMuxer::SupportsMultipleInputChannels()
{
    if (mMediaSource != NULL)
        return mMediaSource->SupportsMultipleInputChannels();
    else
        return false;
}

bool MediaSourceMuxer::SelectInputChannel(int pIndex)
{
    if (mMediaSource != NULL)
        return mMediaSource->SelectInputChannel(pIndex);
    else
        return false;
}

string MediaSourceMuxer::CurrentInputChannel()
{
    if (mMediaSource != NULL)
        return mMediaSource->CurrentInputChannel();
    else
        return "";
}

list<string> MediaSourceMuxer::GetInputChannels()
{
    list<string> tNone;

    if (mMediaSource != NULL)
        return mMediaSource->GetInputChannels();
    else
        return tNone;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
