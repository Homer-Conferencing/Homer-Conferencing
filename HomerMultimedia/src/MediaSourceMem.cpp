/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a ffmpeg based memory media source
 * Author:  Thomas Volkert
 * Since:   2011-05-05
 */

#include <MediaSourceMem.h>
#include <MediaSource.h>
#include <ProcessStatisticService.h>
#include <RTP.h>
#include <Logger.h>

#include <string>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

MediaSourceMem::MediaSourceMem(bool pRtpActivated):
    MediaSource("MEM-IN:"), RTP()
{
    mSourceType = SOURCE_MEMORY;
    mStreamPacketBuffer = (char*)malloc(MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE);
    mFragmentBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);
    mPacketNumber = 0;
    mPacketStatAdditionalFragmentSize = 0;
    mOpenInputStream = false;
    mRtpActivated = pRtpActivated;
    RTPRegisterPacketStatistic(this);
    mDecoderFifo = new MediaFifo(MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT, MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE, "MediaSourceMem");

    mStreamCodecId = CODEC_ID_NONE;

	LOG(LOG_VERBOSE, "Listen for video/audio frames from memory with queue of %d bytes", MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT * MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);
}

MediaSourceMem::~MediaSourceMem()
{
    StopGrabbing();

    if (mMediaSourceOpened)
        CloseGrabDevice();

    delete mDecoderFifo;
    free(mStreamPacketBuffer);
    free(mFragmentBuffer);
}

///////////////////////////////////////////////////////////////////////////////
// static call back function:
//      function reads frame packet and delivers it upwards to ffmpeg library
// HINT: result can consist of many network packets, which represent one frame (packet)
int MediaSourceMem::GetNextPacket(void *pOpaque, uint8_t *pBuffer, int pBufferSize)
{
	MediaSourceMem *tMediaSourceMemInstance = (MediaSourceMem*)pOpaque;
    char *tBuffer = (char*)pBuffer;
    ssize_t tBufferSize = (ssize_t) pBufferSize;

    if (tMediaSourceMemInstance->mRtpActivated)
    {// rtp is active, fragmentation possible!
     // we have to parse every incoming network packet and create a frame packet from the network packets (fragments)
        char *tFragmentData;
        ssize_t tFragmentBufferSize;
        unsigned int tFragmentDataSize;
        bool tLastFragment;
        bool tFragmentIsOkay;
        bool tFragmentIsSenderReport;

        tBufferSize = 0;

        do{
            tLastFragment = false;
            tFragmentIsOkay = false;
            tFragmentData = &tMediaSourceMemInstance->mFragmentBuffer[0];
            tFragmentBufferSize = MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE; // maximum size of one single fragment of a frame packet
            // receive a fragment
            tMediaSourceMemInstance->ReadFragment(tFragmentData, tFragmentBufferSize);
            #ifdef MSMEM_DEBUG_PACKETS
                LOGEX(MediaSourceMem, LOG_VERBOSE, "Got packet fragment of size %d at address %p", tFragmentBufferSize, tFragmentData);
            #endif
//            for (unsigned int i = 0; i < 64; i++)
//                if (tFragmentBufferSize >= i)
//                    LOG(LOG_VERBOSE, "stream data (%2u): RTP+12 %02hx(%3d)  RTP %02hx(%3d)", i, tFragmentData[i + 12] & 0xFF, tFragmentData[i + 12] & 0xFF, tFragmentData[i] & 0xFF, tFragmentData[i] & 0xFF);
            if (tMediaSourceMemInstance->mGrabbingStopped)
            {
                LOGEX(MediaSourceMem, LOG_VERBOSE, "Grabbing was stopped meanwhile");
                return -1; //force negative resulting buffer size to signal error and force a return to the calling GUI!
            }
            if (tFragmentBufferSize == 0)
            {
            	LOGEX(MediaSourceMem, LOG_VERBOSE, "Received empty fragment");
                return 0;
            }
            if (tFragmentBufferSize < 0)
            {
                LOGEX(MediaSourceMem, LOG_VERBOSE, "Received invalid fragment");
                return 0;
            }
            tFragmentDataSize = (unsigned int)tFragmentBufferSize;
            // parse and remove the RTP header, extract the encapsulated frame fragment
            tFragmentIsOkay = tMediaSourceMemInstance->RtpParse(tFragmentData, tFragmentDataSize, tLastFragment, tFragmentIsSenderReport, tMediaSourceMemInstance->mStreamCodecId, false);

            // relay new data to registered sinks
            if(tFragmentIsOkay)
            {
                // relay the received fragment to registered sinks
                tMediaSourceMemInstance->RelayPacketToMediaSinks(tFragmentData, tFragmentDataSize);
            }

            if ((tFragmentIsOkay) && (!tFragmentIsSenderReport))
            {
                if (tBufferSize + (ssize_t)tFragmentDataSize < pBufferSize)
                {
                    if (tFragmentDataSize > 0)
                    {
                        // copy the fragment to the final buffer
                        memcpy(tBuffer, tFragmentData, tFragmentDataSize);
                        tBuffer += tFragmentDataSize;
                        tBufferSize += (ssize_t)tFragmentDataSize;
                    }
                    #ifdef MSMEM_DEBUG_PACKETS
                        LOGEX(MediaSourceMem, LOG_VERBOSE, "Resulting temporary buffer size: %d", tBufferSize);
                    #endif
                }else
                {
                    tLastFragment = false;
                    tBuffer = (char*)pBuffer;
                    tBufferSize = (ssize_t) pBufferSize;
                    LOGEX(MediaSourceMem, LOG_ERROR, "Stream buffer too small for input, dropping received stream");
                }
            }else
            {
                if (!tFragmentIsSenderReport)
                    LOGEX(MediaSourceMem, LOG_WARN, "Current RTP packet was reported as invalid by RTP parser, ignoring this data");
                else
                {
                    #ifdef MSMEM_DEBUG_PACKETS
                        LOGEX(MediaSourceMem, LOG_VERBOSE, "Sender report found in data stream");
                    #endif
                }
            }
        }while(!tLastFragment);
    }else
    {// rtp is inactive, no fragmentation!
        tMediaSourceMemInstance->ReadFragment(tBuffer, tBufferSize);
        if (tMediaSourceMemInstance->mGrabbingStopped)
        {
            LOGEX(MediaSourceMem, LOG_VERBOSE, "Grabbing was stopped meanwhile");
            return -1; //force negative resulting buffer size to signal error and force a return to the calling GUI!
        }

        if (tBufferSize < 0)
        {
        	LOGEX(MediaSourceMem, LOG_ERROR, "Error when receiving network data");
            return 0;
        }

        // relay the received fragment to registered sinks
        tMediaSourceMemInstance->RelayPacketToMediaSinks(tBuffer, (unsigned int)tBufferSize);
    }

    #ifdef MSMEM_DEBUG_PACKETS
        LOGEX(MediaSourceMem, LOG_VERBOSE, "Returning frame of size %d", tBufferSize);
    #endif

    return tBufferSize;
}

void MediaSourceMem::WriteFragment(char *pBuffer, int pBufferSize)
{
	if (pBufferSize > 0)
	{
        // log statistics
        // mFragmentHeaderSize to add additional TCPFragmentHeader to the statistic if TCP is used, this is triggered by MediaSourceNet
        AnnouncePacket((int)pBufferSize + mPacketStatAdditionalFragmentSize);
	}
	
    mDecoderFifo->WriteFifo(pBuffer, pBufferSize);
}

void MediaSourceMem::ReadFragment(char *pData, ssize_t &pDataSize)
{
    int tDataSize = pDataSize;
    mDecoderFifo->ReadFifo(&pData[0], tDataSize);
    pDataSize = tDataSize;

    if (pDataSize > 0)
    {
        #ifdef MSMEM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Delivered packet with number %5u at %p with size: %5d", (unsigned int)++mPacketNumber, pData, (int)pDataSize);
        #endif
    }

    // is FIFO near overload situation?
    if (mDecoderFifo->GetUsage() >= MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT - 4)
    {
        LOG(LOG_WARN, "Decoder FIFO is near overload situation, deleting all stored frames");

        // delete all stored frames: it is a better for the decoding!
        mDecoderFifo->ClearFifo();
    }
}

GrabResolutions MediaSourceMem::GetSupportedVideoGrabResolutions()
{
    VideoFormatDescriptor tFormat;

    mSupportedVideoFormats.clear();

    if (mMediaType == MEDIA_VIDEO)
    {
        tFormat.Name="SQCIF";      //      128 ×  96
        tFormat.ResX = 128;
        tFormat.ResY = 96;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="QCIF";       //      176 × 144
        tFormat.ResX = 176;
        tFormat.ResY = 144;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="CIF";        //      352 × 288
        tFormat.ResX = 352;
        tFormat.ResY = 288;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="VGA";       //
        tFormat.ResX = 640;
        tFormat.ResY = 480;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="CIF4";       //      704 × 576
        tFormat.ResX = 704;
        tFormat.ResY = 576;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="DVD";        //      720 × 576
        tFormat.ResX = 720;
        tFormat.ResY = 576;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="SVGA";       //
        tFormat.ResX = 800;
        tFormat.ResY = 600;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="XGA";       //
        tFormat.ResX = 1024;
        tFormat.ResY = 768;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="CIF9";       //     1056 × 864
        tFormat.ResX = 1056;
        tFormat.ResY = 864;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="SXGA";       //     1280 × 1024
        tFormat.ResX = 1280;
        tFormat.ResY = 1024;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="WXGA+";      //     1440 × 900
        tFormat.ResX = 1440;
        tFormat.ResY = 900;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="SXGA+";       //     1440 × 1050
        tFormat.ResX = 1440;
        tFormat.ResY = 1050;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="CIF16";      //     1408 × 1152
        tFormat.ResX = 1408;
        tFormat.ResY = 1152;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="HDTV";       //     1920 × 1080
        tFormat.ResX = 1920;
        tFormat.ResY = 1080;
        mSupportedVideoFormats.push_back(tFormat);

        if (mMediaSourceOpened)
        {
            tFormat.Name="Original";
            tFormat.ResX = mCodecContext->width;
            tFormat.ResY = mCodecContext->height;
            mSupportedVideoFormats.push_back(tFormat);
        }
    }

    return mSupportedVideoFormats;
}

bool MediaSourceMem::SupportsRecording()
{
	return true;
}

bool MediaSourceMem::SupportsRelaying()
{
    return true;
}

int MediaSourceMem::GetChunkDropCounter()
{
    if (mRtpActivated)
        return GetLostPacketsFromRTP();
    else
        return mChunkDropCounter;
}

int MediaSourceMem::GetFragmentBufferCounter()
{
    if (mDecoderFifo != NULL)
        return mDecoderFifo->GetUsage();
    else
        return 0;
}

int MediaSourceMem::GetFragmentBufferSize()
{
    return MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT;
}

bool MediaSourceMem::SetInputStreamPreferences(std::string pStreamCodec, bool pDoReset, bool pRtpActivated)
{
    bool tResult = false;
    enum CodecID tStreamCodecId = MediaSource::FfmpegName2FfmpegId(MediaSource::CodecName2FfmpegName(pStreamCodec));

    if ((mStreamCodecId != tStreamCodecId) ||
        (mRtpActivated != pRtpActivated))
    {
        LOG(LOG_VERBOSE, "Setting new input streaming preferences");

        tResult = true;

        // set new codec
        LOG(LOG_VERBOSE, "    ..stream codec: %d => %d (%s)", mStreamCodecId, tStreamCodecId, pStreamCodec.c_str());
        mStreamCodecId = tStreamCodecId;

        // set RTP encapsulation state
        LOG(LOG_VERBOSE, "    ..stream rtp encapsulation: %d => %d", GetRtpActivation(), pRtpActivated);
        SetRtpActivation(pRtpActivated);

        if ((pDoReset) && (mMediaSourceOpened))
        {
            LOG(LOG_VERBOSE, "Do reset now...");
            Reset();
        }
    }

    return tResult;
}

bool MediaSourceMem::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    int                 tResult;
    ByteIOContext       *tByteIoContext;
    AVInputFormat       *tFormat;
    AVCodec             *tCodec;

    if (pFps > 29.97)
        pFps = 29.97;
    if (pFps < 5)
        pFps = 5;

    LOG(LOG_VERBOSE, "Trying to open the video source");

    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return false;
    }

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(MEM)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    if (mMediaSourceOpened)
    {
        LOG(LOG_ERROR, "Source already open");
        return false;
    }

	// build corresponding "ByteIOContext"
    tByteIoContext = avio_alloc_context((uint8_t*) mStreamPacketBuffer, MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE, /* read-only */0, this, GetNextPacket, NULL, NULL);

    tByteIoContext->seekable = 0;
    // limit packet size, otherwise ffmpeg will deliver unpredictable results ;)
    tByteIoContext->max_packet_size = MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE;

    // find format
    string tCodecName = FfmpegId2FfmpegFormat(mStreamCodecId);
    // ffmpeg knows only the mpegvideo demuxer which is responsible for both MPEG1 and MPEG2 streams
    if ((tCodecName == "mpeg1video") || (tCodecName == "mpeg2video"))
        tCodecName = "mpegvideo";
    LOG(LOG_VERBOSE, "Going to find input format for codec \"%s\"..", tCodecName.c_str());
    tFormat = av_find_input_format(tCodecName.c_str());

    if (tFormat == NULL)
    {
        if (!mGrabbingStopped)
            LOG(LOG_ERROR, "Couldn't find video input format for codec %d", mStreamCodecId);
        return false;
    }
    LOG(LOG_VERBOSE, "Successfully found input format");

    // Open video stream
    LOG(LOG_VERBOSE, "Going to open input stream..");
    mOpenInputStream = true;
    if((tResult = av_open_input_stream(&mFormatContext, tByteIoContext, "", tFormat, NULL)) != 0)
    {
        if (!mGrabbingStopped)
            LOG(LOG_ERROR, "Couldn't open video input stream because of \"%s\".", strerror(AVUNERROR(tResult)));
        else
            LOG(LOG_VERBOSE, "Grabbing was stopped meanwhile");
        mOpenInputStream = false;
        return false;
    }
    mOpenInputStream = false;
    if (mGrabbingStopped)
    {
        LOG(LOG_VERBOSE, "Grabbing already stopped, will return immediately");
        return false;
    }
    LOG(LOG_VERBOSE, "Successfully opened input stream");

    // limit frame analyzing time for ffmpeg internal codec auto detection
    mFormatContext->max_analyze_duration = AV_TIME_BASE / 4; //  1/4 recorded seconds
    // verbose timestamp debugging    mFormatContext->debug = FF_FDEBUG_TS;

    // Retrieve stream information
    LOG(LOG_VERBOSE, "Going to find stream info..");
    if ((tResult = av_find_stream_info(mFormatContext)) < 0)
    {
        if (!mGrabbingStopped)
            LOG(LOG_ERROR, "Couldn't find video stream information because of \"%s\".", strerror(AVUNERROR(tResult)));
        else
            LOG(LOG_VERBOSE, "Grabbing was stopped meanwhile");
        // Close the video stream
        av_close_input_stream(mFormatContext);
        return false;
    }
    LOG(LOG_VERBOSE, "Successfully found stream info");

    // Find the first video stream
    mMediaStreamIndex = -1;
    LOG(LOG_VERBOSE, "Going to find fitting stream..");
    for (int i = 0; i < (int)mFormatContext->nb_streams; i++)
    {
        if(mFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            mMediaStreamIndex = i;
            break;
        }
    }
    if (mMediaStreamIndex == -1)
    {
        LOG(LOG_ERROR, "Couldn't find a video stream");
        // Close the video stream
        av_close_input_stream(mFormatContext);
        return false;
    }

    // Dump information about device file
    av_dump_format(mFormatContext, mMediaStreamIndex, "MediaSourceMem (video)", false);
    //LOG(LOG_VERBOSE, "    ..video stream found with ID: %d, number of available streams: %d", mMediaStreamIndex, mFormatContext->nb_streams);

    // Get a pointer to the codec context for the video stream
    mCodecContext = mFormatContext->streams[mMediaStreamIndex]->codec;

    // set quant. parameters
    mCodecContext->qmin = 1; // default is 2

    // set grabbing resolution and frame-rate to the resulting ones delivered by opened video codec
    mSourceResX = mCodecContext->width;
    mSourceResY = mCodecContext->height;
    mFrameRate = (float)mFormatContext->streams[mMediaStreamIndex]->r_frame_rate.num / mFormatContext->streams[mMediaStreamIndex]->r_frame_rate.den;
    LOG(LOG_INFO, "Detected frame rate: %f", mFrameRate);

    // Find the decoder for the video stream
    LOG(LOG_VERBOSE, "Going to find decoder..");
    if((tCodec = avcodec_find_decoder(mCodecContext->codec_id)) == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find a fitting video codec");
        // Close the video stream
        av_close_input_stream(mFormatContext);
        return false;
    }
    LOG(LOG_VERBOSE, "Successfully found decoder");

    LOG(LOG_VERBOSE, "Going to open video codec..");

    // Open codec
    if ((tResult = HM_avcodec_open(mCodecContext, tCodec)) < 0)
    {
        LOG(LOG_ERROR, "Couldn't open video codec because of \"%s\".", strerror(AVUNERROR(tResult)));
        return false;
    }
    LOG(LOG_VERBOSE, "Successfully opened codec");

    //HINT: we need to allow that input bit stream might be truncated at packet boundaries instead of frame boundaries, otherwise an UDP/TCP based transmission will fail because the decoder expects only complete packets as input
    mCodecContext->flags2 |= CODEC_FLAG2_CHUNKS;

    // allocate software scaler context
    LOG(LOG_VERBOSE, "Going to create scaler context..");
    mScalerContext = sws_getContext(mCodecContext->width, mCodecContext->height, mCodecContext->pix_fmt, mTargetResX, mTargetResY, PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);

    // seek to the current position and drop data received during codec auto detect phase
    av_seek_frame(mFormatContext, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->cur_dts, AVSEEK_FLAG_ANY);

    mMediaType = MEDIA_VIDEO;
    MarkOpenGrabDeviceSuccessful();
    mResXLastGrabbedFrame = 0;
    mResYLastGrabbedFrame = 0;

    return true;
}

bool MediaSourceMem::OpenAudioGrabDevice(int pSampleRate, bool pStereo)
{
    int                 tResult;
    ByteIOContext       *tByteIoContext;
    AVInputFormat       *tFormat;
    AVCodec             *tCodec;

    LOG(LOG_VERBOSE, "Trying to open the audio source");

    if (mMediaType == MEDIA_VIDEO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return false;
    }

    SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(MEM)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);

    if (mMediaSourceOpened)
        return false;

	// build corresponding "ByteIOContex
    tByteIoContext = avio_alloc_context((uint8_t*) mStreamPacketBuffer, MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE, /* read-only */0, this, GetNextPacket, NULL, NULL);

    tByteIoContext->seekable = 0;
    // limit packet size
    tByteIoContext->max_packet_size = MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE;

    // find format
    tFormat = av_find_input_format(FfmpegId2FfmpegFormat(mStreamCodecId).c_str());

    if (tFormat == NULL)
    {
        if (!mGrabbingStopped)
            LOG(LOG_ERROR, "Couldn't find audio input format for codec %d", mStreamCodecId);
        return false;
    }
    LOG(LOG_VERBOSE, "Successfully found input format");

    // Open audio stream
    mOpenInputStream = true;
    if((tResult = av_open_input_stream(&mFormatContext, tByteIoContext, "", tFormat, NULL)) != 0)
    {
        if (!mGrabbingStopped)
            LOG(LOG_ERROR, "Couldn't open audio input stream because of \"%s\".", strerror(AVUNERROR(tResult)));
        else
            LOG(LOG_VERBOSE, "Grabbing was stopped meanwhile");
        mOpenInputStream = false;
        return false;
    }
    mOpenInputStream = false;
    if (mGrabbingStopped)
    {
        LOG(LOG_VERBOSE, "Grabbing already stopped, will return immediately");
        return false;
    }
    LOG(LOG_VERBOSE, "Successfully opened input stream");

    // limit frame analyzing time for ffmpeg internal codec auto detection
    mFormatContext->max_analyze_duration = AV_TIME_BASE / 4; //  1/4 recorded seconds
    // verbose timestamp debugging    mFormatContext->debug = FF_FDEBUG_TS;

    // Retrieve stream information
    if ((tResult = av_find_stream_info(mFormatContext)) < 0)
    {
        if (!mGrabbingStopped)
            LOG(LOG_ERROR, "Couldn't find audio stream information because of \"%s\".", strerror(AVUNERROR(tResult)));
        else
            LOG(LOG_VERBOSE, "Grabbing was stopped meanwhile");
        // Close the audio stream
        av_close_input_stream(mFormatContext);
        return false;
    }
    LOG(LOG_VERBOSE, "Successfully found stream info");

    // Find the first audio stream
    mMediaStreamIndex = -1;
    for (int i = 0; i < (int)mFormatContext->nb_streams; i++)
    {
        if(mFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            mMediaStreamIndex = i;
            break;
        }
    }
    if (mMediaStreamIndex == -1)
    {
        LOG(LOG_ERROR, "Couldn't find an audio stream");
        // Close the audio stream
        av_close_input_stream(mFormatContext);
        return false;
    }

    // Dump information about device file
    av_dump_format(mFormatContext, mMediaStreamIndex, "MediaSourceMem (audio)", false);
    //printf("    ..audio stream found with ID: %d, number of available streams: %d\n", mMediaStreamIndex, mFormatContext->nb_streams);

    // Get a pointer to the codec context for the audio stream
    mCodecContext = mFormatContext->streams[mMediaStreamIndex]->codec;

    // force signed 16 bit sample format
    //mCodecContext->sample_fmt = SAMPLE_FMT_S16;

    // set sample rate and bit rate to the resulting ones delivered by opened audio codec
    mSampleRate = mCodecContext->sample_rate;
    mStereo = pStereo;

    // Find the decoder for the audio stream
    if((tCodec = avcodec_find_decoder(mCodecContext->codec_id)) == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find a fitting codec");
        // Close the audio stream
        av_close_input_stream(mFormatContext);
        return false;
    }

    // Inform the codec that we can handle truncated bitstreams -- i.e.,
    // bitstreams where frame boundaries can fall in the middle of packets
//    if(tCodec->capabilities & CODEC_CAP_TRUNCATED)
//        mCodecContext->flags |= CODEC_FLAG_TRUNCATED;

    LOG(LOG_VERBOSE, "Going to open audio codec..");

    // Open codec
    if ((tResult = HM_avcodec_open(mCodecContext, tCodec)) < 0)
    {
        LOG(LOG_ERROR, "Couldn't open audio codec because of \"%s\".", strerror(AVUNERROR(tResult)));
        // Close the audio stream
        av_close_input_stream(mFormatContext);
        return false;
    }

    // seek to the current position and drop data received during codec auto detect phase
    av_seek_frame(mFormatContext, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->cur_dts, AVSEEK_FLAG_ANY);

    mMediaType = MEDIA_AUDIO;
    MarkOpenGrabDeviceSuccessful();

    return true;
}

bool MediaSourceMem::CloseGrabDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close");

    if (mMediaSourceOpened)
    {
        StopRecording();

        mMediaSourceOpened = false;

        // free the software scaler context
        if (mMediaType == MEDIA_VIDEO)
            sws_freeContext(mScalerContext);

        // Close the stream codec
        avcodec_close(mCodecContext);

        // Close the video stream
        av_close_input_stream(mFormatContext);

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mGrabbingStopped = false;
    mMediaType = MEDIA_UNKNOWN;

    ResetPacketStatistic();

    return tResult;
}

int MediaSourceMem::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    AVFrame             *tSourceFrame = NULL, *tRGBFrame = NULL;
    AVPacket            tPacket;
    int                 tFrameFinished = 0;
    int                 tBytesDecoded = 0;

    //LOG(LOG_VERBOSE, "Trying to grab frame");

    // lock grabbing
    mGrabMutex.lock();

    //HINT: maybe unsafe, buffer could be freed between call and mutex lock => task of the application to prevent this
    if (pChunkBuffer == NULL)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed(GetMediaTypeStr() + " grab buffer is NULL");

        return -1;
    }

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed(GetMediaTypeStr() + " source is closed");

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

    // read next frame from video source - blocking
    if (av_read_frame(mFormatContext, &tPacket) != 0)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        if (!mGrabbingStopped)
        {
            // acknowledge failed
            MarkGrabChunkFailed("couldn't read next " + GetMediaTypeStr() + " frame");
        }

        return -1;
    }

    #ifdef MSMEM_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "New read chunk %5d with size: %d and stream index: %d", mChunkNumber + 1, tPacket.size, tPacket.stream_index);
    #endif

    if ((tPacket.data != NULL) && (tPacket.size > 0))
    {
        #ifdef MSMEM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "New packet..");
            LOG(LOG_VERBOSE, "      ..duration: %d", tPacket.duration);
            LOG(LOG_VERBOSE, "      ..pts: %ld", tPacket.pts);
            LOG(LOG_VERBOSE, "      ..dts: %ld", tPacket.dts);
            LOG(LOG_VERBOSE, "      ..size: %d", tPacket.size);
            LOG(LOG_VERBOSE, "      ..pos: %ld", tPacket.pos);
        #endif

        switch(mMediaType)
        {
            case MEDIA_VIDEO:
                    if ((!pDropChunk) || (mRecording))
                    {
//                        #ifdef MSMEM_DEBUG_PACKETS
//                            LOG(LOG_VERBOSE, "Decode video frame..");
//                        #endif

                        // Allocate video frame for source and RGB format
                        if ((tSourceFrame = avcodec_alloc_frame()) == NULL)
                        {
                            // unlock grabbing
                            mGrabMutex.unlock();

                            // acknowledge failed"
                            MarkGrabChunkFailed("out of video memory");

                            return -1;
                        }

                        // Decode the next chunk of data
                        tBytesDecoded = HM_avcodec_decode_video(mCodecContext, tSourceFrame, &tFrameFinished, &tPacket);

                        // emulate set FPS
                        tSourceFrame->pts = FpsEmulationGetPts();

//                        #ifdef MSMEM_DEBUG_PACKETS
//                            LOG(LOG_VERBOSE, "    ..with result(!= 0 => OK): %d bytes: %i\n", tFrameFinished, tBytesDecoded);
//                        #endif

                        // log lost packets: difference between currently received frame number and the number of locally processed frames
                        //HINT: if RTP is active we rely on RTP parser, which automatically calls SetLostPacketCount()
                        if (!mRtpActivated)
                            SetLostPacketCount(tSourceFrame->coded_picture_number - mChunkNumber);

//                        #ifdef MSMEM_DEBUG_PACKETS
//                            LOG(LOG_VERBOSE, "Video frame coded: %d internal frame number: %d", tSourceFrame->coded_picture_number, mChunkNumber);
//                        #endif

                        if ((mResXLastGrabbedFrame != mCodecContext->width) || (mResYLastGrabbedFrame != mCodecContext->height))
                        {
							// check if video resolution has changed within remote GUI
							if ((mSourceResX != mCodecContext->width) || (mSourceResY != mCodecContext->height))
							{
								LOG(LOG_INFO, "Video resolution change at remote side detected");

                                // free the software scaler context
                                sws_freeContext(mScalerContext);

                                // set grabbing resolution to the resolution from the codec, which has automatically detected the new resolution
                                mSourceResX = mCodecContext->width;
                                mSourceResY = mCodecContext->height;

                                // allocate software scaler context
                                mScalerContext = sws_getContext(mCodecContext->width, mCodecContext->height, mCodecContext->pix_fmt, mTargetResX, mTargetResY, PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);

                                LOG(LOG_INFO, "Video resolution changed to %d * %d", mCodecContext->width, mCodecContext->height);
							}

							mResXLastGrabbedFrame = mCodecContext->width;
                            mResYLastGrabbedFrame = mCodecContext->height;
                        }
                    }

                    // re-encode the frame and write it to file
                    if (mRecording)
                        RecordFrame(tSourceFrame);

                    if (!pDropChunk)
                    {
//                        #ifdef MSMEM_DEBUG_PACKETS
//                            LOG(LOG_VERBOSE, "Convert video frame..");
//                        #endif

                        // Allocate video frame for RGB format
                        if ((tRGBFrame = avcodec_alloc_frame()) == NULL)
                        {
                            // Free the YUV frame
                            if (tSourceFrame != NULL)
                                av_free(tSourceFrame);

                            // free packet buffer
                            av_free_packet(&tPacket);

                            // unlock grabbing
                            mGrabMutex.unlock();

                            // acknowledge failed"
                            MarkGrabChunkFailed("out of video memory");

                            return -1;
                        }

                        // Assign appropriate parts of buffer to image planes in tRGBFrame
                        avpicture_fill((AVPicture *)tRGBFrame, (uint8_t *)pChunkBuffer, PIX_FMT_RGB32, mTargetResX, mTargetResY);

                        // convert frame from YUV to RGB format
                        if ((tFrameFinished != 0) && (tBytesDecoded >= 0))
                        {
                            HM_sws_scale(mScalerContext, tSourceFrame->data, tSourceFrame->linesize, 0, mCodecContext->height, tRGBFrame->data, tRGBFrame->linesize);
                        }else
                        {
                            // Free the RGB frame
                            av_free(tRGBFrame);

                            // Free the YUV frame
                            if (tSourceFrame != NULL)
                                av_free(tSourceFrame);

                            // free packet buffer
                            av_free_packet(&tPacket);

                            // unlock grabbing
                            mGrabMutex.unlock();

                            // only print debug output if it is not "operation not permitted"
                            //if ((tBytesDecoded < 0) && (AVUNERROR(tBytesDecoded) != EPERM))

                            // acknowledge failed"
                            MarkGrabChunkFailed("couldn't decode video frame-" + toString(strerror(AVUNERROR(tBytesDecoded))) + "(" + toString(AVUNERROR(tBytesDecoded)) + ")");

                            return -1;
                        }
                        #ifdef MSMEM_DEBUG_PACKETS
                            LOG(LOG_VERBOSE, "New video frame..");
                            LOG(LOG_VERBOSE, "      ..key frame: %d", tSourceFrame->key_frame);
                            switch(tSourceFrame->pict_type)
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
                                        LOG(LOG_VERBOSE, "      ..picture type: %d", tSourceFrame->pict_type);
                                        break;
                            }
                            LOG(LOG_VERBOSE, "      ..pts: %ld", tSourceFrame->pts);
                            LOG(LOG_VERBOSE, "      ..coded pic number: %d", tSourceFrame->coded_picture_number);
                            LOG(LOG_VERBOSE, "      ..display pic number: %d", tSourceFrame->display_picture_number);
                        #endif

                        // return size of decoded frame
                        pChunkSize = avpicture_get_size(PIX_FMT_RGB32, mTargetResX, mTargetResY) * sizeof(uint8_t);

                        // Free the RGB frame
                        av_free(tRGBFrame);
                    }

                    // Free the YUV frame
                    if (tSourceFrame != NULL)
                        av_free(tSourceFrame);

                    break;

            case MEDIA_AUDIO:
                    if ((!pDropChunk) || (mRecording))
                    {
                        //printf("DecodeFrame..\n");
                        // Decode the next chunk of data
                        int tOutputBufferSize = MEDIA_SOURCE_SAMPLES_MULTI_BUFFER_SIZE;
                        int tBytesDecoded = HM_avcodec_decode_audio(mCodecContext, (int16_t *)pChunkBuffer, &tOutputBufferSize, &tPacket);
                        pChunkSize = tOutputBufferSize;

                        // re-encode the frame and write it to file
                        if (mRecording)
                            RecordSamples((int16_t *)pChunkBuffer, pChunkSize);

                        //printf("    ..with result: %d bytes decoded into %d bytes\n", tBytesDecoded, tOutputBufferSize);

                        if ((tBytesDecoded < 0) || (tOutputBufferSize == 0))
                        {
                            // free packet buffer
                            av_free_packet(&tPacket);

                            // unlock grabbing
                            mGrabMutex.unlock();

                            // only print debug output if it is not "operation not permitted"
                            //if (AVUNERROR(tBytesDecoded) != EPERM)
                            // acknowledge failed"
                            MarkGrabChunkFailed("couldn't decode audio frame because - \"" + toString(strerror(AVUNERROR(tBytesDecoded))) + "(" + toString(AVUNERROR(tBytesDecoded)) + ")\"");

                            return -1;
                        }
                    }
                    break;

            default:
                    LOG(LOG_ERROR, "Media type unknown");
                    break;

        }
    }else
        LOG(LOG_ERROR, "Empty packet received");

    // free packet buffer
    av_free_packet(&tPacket);

    // unlock grabbing
    mGrabMutex.unlock();

    mChunkNumber++;

    // acknowledge success
    MarkGrabChunkSuccessful(mChunkNumber);

    return mChunkNumber;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
