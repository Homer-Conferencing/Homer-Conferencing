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

// maximum packet size, including encoded data and RTP/TS
#define MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE          MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE

///////////////////////////////////////////////////////////////////////////////

MediaSourceMem::MediaSourceMem(bool pRtpActivated):
    MediaSource("MEM-IN:"), RTP()
{
	mResXLastGrabbedFrame = 0;
	mResYLastGrabbedFrame = 0;
	mWrappingHeaderSize= 0;
	mSourceFrame = NULL;
	mRGBFrame = NULL;
    mSourceType = SOURCE_MEMORY;
    mStreamPacketBuffer = (char*)malloc(MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE);
    mFragmentBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);
    mFragmentNumber = 0;
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

    #ifdef MSMEM_DEBUG_PACKETS
        LOGEX(MediaSourceMem, LOG_VERBOSE, "Got a call for GetNextPacket() with a packet buffer at %p and size of %d bytes", pBuffer, pBufferSize);
    #endif
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
                    LOGEX(MediaSourceMem, LOG_ERROR, "Stream buffer of %d bytes too small for input, dropping received stream", pBufferSize);
                }
            }else
            {
                if (!tFragmentIsSenderReport)
                    LOGEX(MediaSourceMem, LOG_WARN, "Current RTP packet was reported as invalid by RTP parser, ignoring this data");
                else
                {
                    int tPackets = 0;
                    int tOctets = 0;
                    if (tMediaSourceMemInstance->RtcpParse(tFragmentData, tFragmentDataSize, tPackets, tOctets))
                    {
						#ifdef MSMEM_DEBUG_SENDER_REPORTS
                    		RTP::LogRtcpHeader((RtcpHeader*)tFragmentData);
                    		LOGEX(MediaSourceMem, LOG_VERBOSE, "Sender reports: %d packets and %d bytes transmitted", tPackets, tOctets);
						#endif
                    }else
                        LOGEX(MediaSourceMem, LOG_ERROR, "Unable to parse sender report in received RTCP packet");
                }
            }
        }while(!tLastFragment);
    }else
    {// rtp is inactive
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
	
    if (mDecoderFifo->GetUsage() >= MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT - 4)
    {
        LOG(LOG_WARN, "Decoder FIFO is near overload situation in WriteFragmet(), deleting all stored frames");

        // delete all stored frames: it is a better for the decoding!
        mDecoderFifo->ClearFifo();
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
            LOG(LOG_VERBOSE, "Delivered fragment with number %5u at %p with size %5d towards decoder", (unsigned int)++mFragmentNumber, pData, (int)pDataSize);
        #endif
    }

    // is FIFO near overload situation?
    if (mDecoderFifo->GetUsage() >= MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT - 4)
    {
        LOG(LOG_WARN, "Decoder FIFO is near overload situation in ReadFragment(), deleting all stored frames");

        // delete all stored frames: it is a better for the decoding!
        mDecoderFifo->ClearFifo();
    }
}

bool MediaSourceMem::SupportsDecoderFrameStatistics()
{
    return (mMediaType == MEDIA_VIDEO);
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

void MediaSourceMem::StopGrabbing()
{
	MediaSource::StopGrabbing();

	LOG(LOG_VERBOSE, "Going to stop memory based %s source", GetMediaTypeStr().c_str());

    char tData[4];
    mDecoderFifo->WriteFifo(tData, 0);
    mDecoderFifo->WriteFifo(tData, 0);

    LOG(LOG_VERBOSE, "Memory based %s source successfully stopped", GetMediaTypeStr().c_str());
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

bool MediaSourceMem::SetInputStreamPreferences(std::string pStreamCodec, bool pDoReset)
{
    bool tResult = false;
    enum CodecID tStreamCodecId = GetCodecIDFromGuiName(pStreamCodec);

    if (mStreamCodecId != tStreamCodecId)
    {
        LOG(LOG_VERBOSE, "Setting new input streaming preferences");

        tResult = true;

        // set new codec
        LOG(LOG_VERBOSE, "    ..stream codec: %d => %d (%s)", mStreamCodecId, tStreamCodecId, pStreamCodec.c_str());
        mStreamCodecId = tStreamCodecId;

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
    AVIOContext         *tIoContext;
    AVInputFormat       *tFormat;

    mMediaType = MEDIA_VIDEO;

    if (pFps > 29.97)
        pFps = 29.97;
    if (pFps < 5)
        pFps = 5;

    LOG(LOG_VERBOSE, "Trying to open the video source");

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(MEM)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    // there is no differentiation between H.263+ and H.263 when decoding an incoming video stream
    if (mStreamCodecId == CODEC_ID_H263P)
        mStreamCodecId = CODEC_ID_H263;

    // get a format description
    if (!DescribeInput(mStreamCodecId, &tFormat))
    	return false;

	// build corresponding "AVIOContext"
    CreateIOContext(mStreamPacketBuffer, MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE, GetNextPacket, NULL, this, &tIoContext);

    // open the input for the described format and via the provided I/O control
    mOpenInputStream = true;
    bool tRes = OpenInput("", tFormat, tIoContext);
    mOpenInputStream = false;
    if (!tRes)
    	return false;

    // detect all available video/audio streams in the input
    if (!DetectAllStreams())
    	return false;

    // select the first matching stream according to mMediaType
    if (!SelectStream())
    	return false;

    // finds and opens the correct decoder
    if (!OpenDecoder())
    	return false;

    // overwrite FPS by the timebase of the selected codec
    mFrameRate = (float)mFormatContext->streams[mMediaStreamIndex]->codec->time_base.den / mFormatContext->streams[mMediaStreamIndex]->codec->time_base.num;

    // allocate software scaler context
    LOG(LOG_VERBOSE, "Going to create video scaler context..");
    mScalerContext = sws_getContext(mCodecContext->width, mCodecContext->height, mCodecContext->pix_fmt, mTargetResX, mTargetResY, PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);

    // seek to the current position and drop data received during codec auto detect phase
    av_seek_frame(mFormatContext, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->cur_dts, AVSEEK_FLAG_ANY);

    // Allocate video frame for source and RGB format
    if ((mSourceFrame = avcodec_alloc_frame()) == NULL)
        return false;
    if ((mRGBFrame = avcodec_alloc_frame()) == NULL)
        return false;

    MarkOpenGrabDeviceSuccessful();

    mResXLastGrabbedFrame = 0;
    mResYLastGrabbedFrame = 0;
    mDecodedIFrames = 0;
    mDecodedPFrames = 0;
    mDecodedBFrames = 0;

    return true;
}

bool MediaSourceMem::OpenAudioGrabDevice(int pSampleRate, bool pStereo)
{
    AVIOContext       	*tIoContext;
    AVInputFormat       *tFormat;

    mMediaType = MEDIA_AUDIO;

    LOG(LOG_VERBOSE, "Trying to open the audio source");

    SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(MEM)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);

    // get a format description
    if (!DescribeInput(mStreamCodecId, &tFormat))
    	return false;

	// build corresponding "AVIOContext"
    CreateIOContext(mStreamPacketBuffer, MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE, GetNextPacket, NULL, this, &tIoContext);

    // open the input for the described format and via the provided I/O control
    mOpenInputStream = true;
    bool tRes = OpenInput("", tFormat, tIoContext);
    mOpenInputStream = false;
    if (!tRes)
    	return false;

    // detect all available video/audio streams in the input
    if (!DetectAllStreams())
    	return false;

    // select the first matching stream according to mMediaType
    if (!SelectStream())
    	return false;

    mStereoInput = pStereo;

    // finds and opens the correct decoder
    if (!OpenDecoder())
    	return false;

    // seek to the current position and drop data received during codec auto detect phase
    av_seek_frame(mFormatContext, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->cur_dts, AVSEEK_FLAG_ANY);

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
        HM_close_input(mFormatContext);

        // Free the frames
        av_free(mRGBFrame);
        av_free(mSourceFrame);

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mGrabbingStopped = false;
    mMediaType = MEDIA_UNKNOWN;

    // reset the FIFO to have a clean FIFO next time we open the media source again
    mDecoderFifo->ClearFifo();

    ResetPacketStatistic();

    return tResult;
}

int MediaSourceMem::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    AVPacket            tPacket;
    int                 tFrameFinished = 0;
    int                 tBytesDecoded = 0;

    #ifdef MSMEM_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Trying to grab frame");
    #endif

    // lock grabbing
    mGrabMutex.lock();

    //HINT: maybe unsafe, buffer could be freed between call and mutex lock => task of the application to prevent this
    if (pChunkBuffer == NULL)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed(GetMediaTypeStr() + " grab buffer is NULL");

        return GRAB_RES_INVALID;
    }

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed(GetMediaTypeStr() + " source is closed");

        return GRAB_RES_EOF;
    }

    if (mGrabbingStopped)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed(GetMediaTypeStr() + " source is paused");

        return GRAB_RES_INVALID;
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

        return GRAB_RES_INVALID;
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
                        #ifdef MSMEM_DEBUG_PACKETS
                            LOG(LOG_VERBOSE, "Decode video frame..");
                        #endif

                        // ############################
                        // ### DECODE FRAME
                        // ############################
                        tBytesDecoded = HM_avcodec_decode_video(mCodecContext, mSourceFrame, &tFrameFinished, &tPacket);

                        // emulate set FPS
                        mSourceFrame->pts = FpsEmulationGetPts();

                        #ifdef MSMEM_DEBUG_PACKETS
                            LOG(LOG_VERBOSE, "    ..with result(!= 0 => OK): %d bytes: %i\n", tFrameFinished, tBytesDecoded);
                        #endif

                        // log lost packets: difference between currently received frame number and the number of locally processed frames
                        //HINT: if RTP is active we rely on RTP parser, which automatically calls SetLostPacketCount()
                        if (!mRtpActivated)
                            SetLostPacketCount(mSourceFrame->coded_picture_number - mChunkNumber);

                        #ifdef MSMEM_DEBUG_PACKETS
                           LOG(LOG_VERBOSE, "Video frame coded: %d internal frame number: %d", mSourceFrame->coded_picture_number, mChunkNumber);
                        #endif


                        // do we have a video codec change at sender side?
                        if (mStreamCodecId != mCodecContext->codec_id)
                        {
                            LOG(LOG_INFO, "Incoming video stream changed codec from %s(%d) to %s(%d)", GetFormatName(mStreamCodecId).c_str(), mStreamCodecId, GetFormatName(mCodecContext->codec_id).c_str(), mCodecContext->codec_id);

                            LOG(LOG_ERROR, "Unsupported video codec change");

                            mStreamCodecId = mCodecContext->codec_id;
                        }

                        // do we have a video resolution change at sender side?
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

                    // do we have valid data from video decoder?
                    if ((tFrameFinished != 0) && (tBytesDecoded >= 0))
                    {
                        // ############################
                        // ### ANNOUNCE FRAME (statistics)
                        // ############################
                        AnnounceFrame(mSourceFrame);

                        // ############################
                        // ### RECORD FRAME
                        // ############################
                        if (mRecording)
                            RecordFrame(mSourceFrame);

                        // ############################
                        // ### SCALE FRAME (CONVERT)
                        // ############################
                        if (!pDropChunk)
                        {
                            #ifdef MSMEM_DEBUG_PACKETS
                                LOG(LOG_VERBOSE, "Convert video frame..");
                            #endif

                            // Assign appropriate parts of buffer to image planes in mRGBFrame
                            avpicture_fill((AVPicture *)mRGBFrame, (uint8_t *)pChunkBuffer, PIX_FMT_RGB32, mTargetResX, mTargetResY);

                            HM_sws_scale(mScalerContext, mSourceFrame->data, mSourceFrame->linesize, 0, mCodecContext->height, mRGBFrame->data, mRGBFrame->linesize);

                            #ifdef MSMEM_DEBUG_PACKETS
                                LOG(LOG_VERBOSE, "New video frame..");
                                LOG(LOG_VERBOSE, "      ..key frame: %d", mSourceFrame->key_frame);
                                switch(mSourceFrame->pict_type)
                                {
                                        case AV_PICTURE_TYPE_I:
                                            LOG(LOG_VERBOSE, "      ..picture type: i-frame");
                                            break;
                                        case AV_PICTURE_TYPE_P:
                                            LOG(LOG_VERBOSE, "      ..picture type: p-frame");
                                            break;
                                        case AV_PICTURE_TYPE_B:
                                            LOG(LOG_VERBOSE, "      ..picture type: b-frame");
                                            break;
                                        default:
                                            LOG(LOG_VERBOSE, "      ..picture type: %d", mSourceFrame->pict_type);
                                            break;
                                }
                                LOG(LOG_VERBOSE, "      ..pts: %ld", mSourceFrame->pts);
                                LOG(LOG_VERBOSE, "      ..coded pic number: %d", mSourceFrame->coded_picture_number);
                                LOG(LOG_VERBOSE, "      ..display pic number: %d", mSourceFrame->display_picture_number);
                            #endif

                            // return size of decoded frame
                            pChunkSize = avpicture_get_size(PIX_FMT_RGB32, mTargetResX, mTargetResY) * sizeof(uint8_t);
                        }
                    }else
                    {// decoder delivered no valid data

                        // unlock grabbing
                        mGrabMutex.unlock();

                        // only print debug output if it is not "operation not permitted"
                        //if ((tBytesDecoded < 0) && (AVUNERROR(tBytesDecoded) != EPERM))

                        if (tBytesDecoded != 0)
                        {
                            // acknowledge failed"
                            if (tPacket.size != tBytesDecoded)
                                MarkGrabChunkFailed("couldn't decode video frame-" + toString(strerror(AVUNERROR(tBytesDecoded))) + "(" + toString(AVUNERROR(tBytesDecoded)) + ")");
                            else
                                MarkGrabChunkFailed("couldn't decode video frame");
                        }else
                        {
                            LOG(LOG_VERBOSE, "Video decoder delivered no frame");

                            MarkGrabChunkSuccessful(mChunkNumber);
                        }

                        // free packet buffer
                        av_free_packet(&tPacket);

                        pChunkSize = 0;

                        return GRAB_RES_INVALID;
                    }
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

                            return GRAB_RES_INVALID;
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
