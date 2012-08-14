/*****************************************************************************
 *
 * Copyright (C) 2012 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a video scaler
 * Author:  Thomas Volkert
 * Since:   2012-08-09
 */

#include <VideoScaler.h>
#include <MediaSourceMuxer.h>
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

VideoScaler::VideoScaler():
    MediaFifo("VideoScaler")
{
    mScalerNeeded = false;
    mInputFifo = NULL;
    mOutputFifo = NULL;
}

VideoScaler::~VideoScaler()
{

}

void VideoScaler::StartScaler(enum CodecID pTargetCodecId, int pSourceResX, int pSourceResY, int pTargetResX, int pTargetResY)
{
    LOG(LOG_VERBOSE, "Starting scaler, converting from resolution %d*%d to %d*%d", pSourceResX, pSourceResY, pTargetResX, pTargetResY);

    mTargetCodecId = pTargetCodecId;
    mSourceResX = pSourceResX;
    mSourceResY = pSourceResY;
    mTargetResX = pTargetResX;
    mTargetResY = pTargetResY;
    if (mTargetCodecId == CODEC_ID_MJPEG)
        mTargetPixelFormat = PIX_FMT_YUVJ420P;
    else
        mTargetPixelFormat = PIX_FMT_YUV420P;

    int tInputBufferSize = avpicture_get_size(PIX_FMT_RGB32, mSourceResX, mSourceResY);
    //HINT: we have to allocate input FIFO here to make sure we can force a return from a read request inside StopScaler(), StartScaler() and StopScaler() should be called from the same thread/context!
    mInputFifo = new MediaFifo(MEDIA_SOURCE_MUX_INPUT_QUEUE_SIZE_LIMIT, tInputBufferSize, "VIDEO-ScalerInput");

    // start scaler main loop
    StartThread();

    while(!mScalerNeeded)
    {
        LOG(LOG_VERBOSE, "Waiting for the start of VIDEO scaling thread");
        Thread::Suspend(100 * 1000);
    }

}

void VideoScaler::StopScaler()
{
    int tSignalingRound = 0;
    char tTmp[4];

    LOG(LOG_VERBOSE, "Stopping scaler");

    if (mInputFifo != NULL)
    {
        // tell scaler thread it isn't needed anymore
        mScalerNeeded = false;

        // wait for termination of scaler thread
        do
        {
            if(tSignalingRound > 0)
                LOG(LOG_WARN, "Signaling round %d to stop scaler, system has high load", tSignalingRound);
            tSignalingRound++;

            // write fake data to awake scaler thread as long as it still runs
            mInputFifo->WriteFifo(tTmp, 0);
        }while(!StopThread(1000));
    }

    delete mInputFifo;
    mInputFifo = NULL;

    LOG(LOG_VERBOSE, "Scaler stopped");
}

void VideoScaler::WriteFifo(char* pBuffer, int pBufferSize)
{
    if (mInputFifo != NULL)
        mInputFifo->WriteFifo(pBuffer, pBufferSize);
}

void VideoScaler::ReadFifo(char *pBuffer, int &pBufferSize)
{
    if (mOutputFifo != NULL)
        mOutputFifo->ReadFifo(pBuffer, pBufferSize);
}

void VideoScaler::ClearFifo()
{
    if (mInputFifo != NULL)
        mInputFifo->ClearFifo();
}

int VideoScaler::ReadFifoExclusive(char **pBuffer, int &pBufferSize)
{
    if ((mInputFifo != NULL) && (mOutputFifo != NULL))
        return mOutputFifo->ReadFifoExclusive(pBuffer, pBufferSize);
    else
    {
        pBufferSize = 0;
        return -1;
    }
}

void VideoScaler::ReadFifoExclusiveFinished(int pEntryPointer)
{
    if (mOutputFifo != NULL)
        mOutputFifo->ReadFifoExclusiveFinished(pEntryPointer);
}

int VideoScaler::GetEntrySize()
{
    if (mInputFifo != NULL)
        return mInputFifo->GetEntrySize();
    else
        return 0;
}

int VideoScaler::GetUsage()
{
    if (mInputFifo != NULL)
        return mInputFifo->GetUsage();
    else
        return 0;
}

int VideoScaler::GetSize()
{
    if (mInputFifo != NULL)
        return mInputFifo->GetSize();
    else
        return 0;
}

void* VideoScaler::Run(void* pArgs)
{
    char                *tBuffer;
    int                 tBufferSize;
    int                 tFifoEntry = 0;
    AVFrame             *tRGBFrame;
    AVFrame             *tYUVFrame;
    uint8_t             *tOutputBuffer;
    /* current chunk */
    int                 tCurrentChunkSize = 0;

    LOG(LOG_VERBOSE, "Video scaling thread started");

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Scaler(" + MediaSource::GetFormatName(mTargetCodecId) + ")");
    int tOutputBufferSize = avpicture_get_size(mTargetPixelFormat, mTargetResX, mTargetResY);

    // allocate chunk buffer
    tOutputBuffer = (uint8_t*)malloc(tOutputBufferSize);

    // Allocate video frame for YUV format
    if ((tYUVFrame = avcodec_alloc_frame()) == NULL)
    {
        // acknowledge failed"
        LOG(LOG_ERROR, "Out of video memory in avcodec_alloc_frame()");
    }

    // Assign appropriate parts of buffer to image planes in tYUVFrame
    avpicture_fill((AVPicture *)tYUVFrame, (uint8_t *)tOutputBuffer, mTargetPixelFormat, mTargetResX, mTargetResY);

    // Allocate video frame for RGB format
    if ((tRGBFrame = avcodec_alloc_frame()) == NULL)
    {
        // acknowledge failed"
        LOG(LOG_ERROR, "Out of video memory in avcodec_alloc_frame()");
    }

    // allocate software scaler context, input/output FIFO
    mScalerContext = sws_getContext(mSourceResX, mSourceResY, PIX_FMT_RGB32, mTargetResX, mTargetResY, mTargetPixelFormat, SWS_BICUBIC, NULL, NULL, NULL);
    mOutputFifo = new MediaFifo(MEDIA_SOURCE_MUX_INPUT_QUEUE_SIZE_LIMIT, tOutputBufferSize, "VIDEO-ScalerOutput");

    mChunkNumber = 0;
    mScalerNeeded = true;

    while(mScalerNeeded)
    {
        if (mInputFifo != NULL)
        {
            tFifoEntry = mInputFifo->ReadFifoExclusive(&tBuffer, tBufferSize);

            if ((tBufferSize > 0) && (mScalerNeeded))
            {
                //HINT: we only get input if mStreamActivated is set and we have some registered media sinks

                tCurrentChunkSize = 0;
                //HINT: media type is always MEDIA_VIDEO here

                // ####################################################################
                // ### PREPARE RGB FRAME
                // ###################################################################
                // Assign appropriate parts of buffer to image planes in tRGBFrame
                avpicture_fill((AVPicture *)tRGBFrame, (uint8_t *)tBuffer, PIX_FMT_RGB32, mSourceResX, mSourceResY);

                // set frame number in corresponding entries within AVFrame structure
                tRGBFrame->pts = mChunkNumber + 1;
                tRGBFrame->coded_picture_number = mChunkNumber + 1;
                tRGBFrame->display_picture_number = mChunkNumber + 1;

                #ifdef VS_DEBUG_PACKETS
                    LOG(LOG_VERBOSE, "SCALER-new input video frame..");
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

                // ####################################################################
                // ### SCALE RGB FRAME (CONVERT)
                // ###################################################################
                int64_t tTime = Time::GetTimeStamp();
                // convert fromn RGB to YUV420
                HM_sws_scale(mScalerContext, tRGBFrame->data, tRGBFrame->linesize, 0, mSourceResY, tYUVFrame->data, tYUVFrame->linesize);
                #ifdef MSM_DEBUG_TIMING
                    int64_t tTime2 = Time::GetTimeStamp();
                    LOG(LOG_VERBOSE, "SCALER-scaling video frame took %ld us", tTime2 - tTime);
                #endif

                //LOG(LOG_VERBOSE, "Video frame data: %p, %p, %p, %p", tYUVFrame->data[0], tYUVFrame->data[1], tYUVFrame->data[2], tYUVFrame->data[3]);
                //LOG(LOG_VERBOSE, "Video frame line size: %d, %d, %d, %d", tYUVFrame->linesize[0], tYUVFrame->linesize[1], tYUVFrame->linesize[2], tYUVFrame->linesize[3]);

                // size of scaled YUV frame
                tCurrentChunkSize = avpicture_get_size(mTargetPixelFormat, mTargetResX, mTargetResY);

                #ifdef VS_DEBUG_PACKETS
                    LOG(LOG_VERBOSE, "SCALER-new output video frame..");
                    LOG(LOG_VERBOSE, "      ..key frame: %d", tYUVFrame->key_frame);
                    switch(tYUVFrame->pict_type)
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
                                LOG(LOG_VERBOSE, "      ..picture type: %d", tYUVFrame->pict_type);
                                break;
                    }
                    LOG(LOG_VERBOSE, "      ..pts: %ld", tYUVFrame->pts);
                    LOG(LOG_VERBOSE, "      ..coded pic number: %d", tYUVFrame->coded_picture_number);
                    LOG(LOG_VERBOSE, "      ..display pic number: %d", tYUVFrame->display_picture_number);
                #endif

                // was there an error during decoding process?
                if (tCurrentChunkSize > 0)
                {// no error
                    // add new chunk to FIFO
                    #ifdef VS_DEBUG_PACKETS
                        LOG(LOG_VERBOSE, "SCALER-writing %d bytes to output FIFO", tCurrentChunkSize);
                    #endif
                    if (tCurrentChunkSize <= mOutputFifo->GetEntrySize())
                    {
                        mOutputFifo->WriteFifo((char*)tOutputBuffer, tCurrentChunkSize);
                        // add meta description about current chunk to different FIFO
                        struct ChunkDescriptor tChunkDesc;
//TODO                            tChunkDesc.Pts = tCurFramePts;
//TODO                            mMetaDataOutputFifo->WriteFifo((char*) &tChunkDesc, sizeof(tChunkDesc));
                        #ifdef VS_DEBUG_PACKETS
                            LOG(LOG_VERBOSE, "SCALER-successful scaler loop");
                        #endif
                    }else
                    {
                        LOG(LOG_ERROR, "Cannot write a VIDEO chunk of %d bytes to the encoder FIFO with %d bytes slots", tCurrentChunkSize, mOutputFifo->GetEntrySize());
                    }
                }
            }else
            {
                // got a message to stop the encoder pipe?
                if (tBufferSize == 0)
                {
                    LOG(LOG_VERBOSE, "Forwarding the empty packet from the scaler input FIFO to the scaler output FIFO");

                    // forward the empty packet to the output FIFO
                    mOutputFifo->WriteFifo(tBuffer, 0);
                }
            }

            // release FIFO entry lock
            if (tFifoEntry >= 0)
                mInputFifo->ReadFifoExclusiveFinished(tFifoEntry);

            // is FIFO near overload situation?
            if (mInputFifo->GetUsage() >= MEDIA_SOURCE_MUX_INPUT_QUEUE_SIZE_LIMIT - 4)
            {
                LOG(LOG_WARN, "Encoder (scaler) FIFO is near overload situation, deleting all stored frames");

                // delete all stored frames: it is a better for the encoding to have a gap instead of frames which have high picture differences
                mInputFifo->ClearFifo();
            }
        }else
        {
            LOG(LOG_VERBOSE, "Suspending the scaler thread for 10 ms");
            Suspend(10 * 1000); // check every 1/100 seconds the state of the FIFO
        }
    }

    LOG(LOG_VERBOSE, "Video scaler left thread main loop");

    delete mOutputFifo;
    mOutputFifo = NULL;

    // free the software scaler context
    sws_freeContext(mScalerContext);

    // Free the RGB frame
    av_free(tRGBFrame);

    // Free the YUV frame
    av_free(tYUVFrame);

    // free the output buffer
    free(tOutputBuffer);

    LOG(LOG_WARN, "Video scaler thread finished");

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
