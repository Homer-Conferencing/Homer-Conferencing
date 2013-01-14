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
#include <MediaSource.h>

#include <string>
#include <stdint.h>

using namespace std;
using namespace Homer::Monitor;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

VideoScaler::VideoScaler(string pName):
    MediaFifo("VideoScaler")
{
	mName = pName;
    mScalerNeeded = false;
    mInputFifo = NULL;
    mOutputFifo = NULL;
    mVideoScalerContext = NULL;
}

VideoScaler::~VideoScaler()
{

}

void VideoScaler::StartScaler(int pInputQueueSize, int pSourceResX, int pSourceResY, enum PixelFormat pSourcePixelFormat, int pTargetResX, int pTargetResY, enum PixelFormat pTargetPixelFormat)
{

    mQueueSize = pInputQueueSize;
    mSourceResX = pSourceResX;
    mSourceResY = pSourceResY;
    mSourcePixelFormat = pSourcePixelFormat;
    mTargetResX = pTargetResX;
    mTargetResY = pTargetResY;
    mTargetPixelFormat = pTargetPixelFormat;

    LOG(LOG_WARN, "Starting %s video scaler, converting resolution %d*%d (fmt: %d) to %d*%d (fmt: %d), queue size: %d", mName.c_str(), pSourceResX, pSourceResY, mSourcePixelFormat, pTargetResX, pTargetResY, mTargetPixelFormat, mQueueSize);

    int tInputBufferSize = avpicture_get_size(mSourcePixelFormat, mSourceResX, mSourceResY) + FF_INPUT_BUFFER_PADDING_SIZE;
    //HINT: we have to allocate input FIFO here to make sure we can force a return from a read request inside StopScaler(), StartScaler() and StopScaler() should be called from the same thread/context!
    mInputFifo = new MediaFifo(mQueueSize, tInputBufferSize, "VIDEO-ScalerInput/" + mName);

    // start scaler main loop
    StartThread();

    while(!mScalerNeeded)
    {
        LOG(LOG_VERBOSE, "Waiting for the start of %s VIDEO scaling thread", mName.c_str());
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
                LOG(LOG_WARN, "Signaling attempt %d to stop video scaler", tSignalingRound);
            tSignalingRound++;

            // write fake data to awake scaler thread as long as it still runs
            mInputFifo->WriteFifo(tTmp, 0);

            Suspend(100 * 1000);
        }while(IsRunning());
    }

    LOG(LOG_VERBOSE, "Video scaler seems to be stopped, deleting input FIFO");

    mInputFifoMutex.lock();
    delete mInputFifo;
    mInputFifo = NULL;
    mInputFifoMutex.unlock();

    LOG(LOG_VERBOSE, "Scaler stopped");
}

void VideoScaler::WriteFifo(char* pBuffer, int pBufferSize)
{
    mInputFifoMutex.lock();
    if (mInputFifo != NULL)
    {
    	if (pBufferSize <= mInputFifo->GetEntrySize())
    		mInputFifo->WriteFifo(pBuffer, pBufferSize);
    	else
    		LOG(LOG_ERROR, "Input buffer of %d bytes is too big for input FIFO of video scaler %s with %d bytes per entry", pBufferSize, mName.c_str(), mInputFifo->GetEntrySize());
    }
    mInputFifoMutex.unlock();
}

void VideoScaler::ReadFifo(char *pBuffer, int &pBufferSize)
{
    if (mOutputFifo != NULL)
        mOutputFifo->ReadFifo(pBuffer, pBufferSize);
    else
        pBufferSize = 0;
}

void VideoScaler::ClearFifo()
{
    mInputFifoMutex.lock();
    if (mInputFifo != NULL)
        mInputFifo->ClearFifo();
    if (mOutputFifo != NULL)
        mOutputFifo->ClearFifo();
    mInputFifoMutex.unlock();
}

int VideoScaler::ReadFifoExclusive(char **pBuffer, int &pBufferSize)
{
    // no locking, context of scaler thread
    if ((mInputFifo != NULL) && (mOutputFifo != NULL))
        return mOutputFifo->ReadFifoExclusive(pBuffer, pBufferSize);
    else
    {
    	LOG(LOG_WARN, "Video scaler not ready yet");
        pBufferSize = 0;
        return -1;
    }
}

void VideoScaler::ReadFifoExclusiveFinished(int pEntryPointer)
{
    // no locking, context of scaler thread
    if (mOutputFifo != NULL)
        mOutputFifo->ReadFifoExclusiveFinished(pEntryPointer);
}

int VideoScaler::GetEntrySize()
{
    int tResult = 0;

    mInputFifoMutex.lock();
    if (mInputFifo != NULL)
        tResult = mInputFifo->GetEntrySize();
    mInputFifoMutex.unlock();

    return tResult;
}

int VideoScaler::GetUsage()
{
    int tResult = 0;

    mInputFifoMutex.lock();
    if (mOutputFifo != NULL)
        tResult += mOutputFifo->GetUsage();
    if (mInputFifo != NULL)
        tResult += mInputFifo->GetUsage();
    mInputFifoMutex.unlock();

    return tResult;
}

int VideoScaler::GetSize()
{
    int tResult = 0;

    mInputFifoMutex.lock();
    if (mInputFifo != NULL)
        tResult = mInputFifo->GetSize();
    mInputFifoMutex.unlock();

    return tResult;
}

void VideoScaler::ChangeInputResolution(int pResX, int pResY)
{
    LOG(LOG_VERBOSE, "Changing input resolution to %d*%d..", pResX, pResY);

    StopScaler();

    // set grabbing resolution to the resolution from the codec, which has automatically detected the new resolution
    mSourceResX = pResX;
    mSourceResY = pResY;

    // restart video scaler with new settings
    StartScaler(mQueueSize, mSourceResX, mSourceResY, mSourcePixelFormat, mTargetResX, mTargetResY, mTargetPixelFormat);

    LOG(LOG_VERBOSE, "Input resolution changed");
}

void* VideoScaler::Run(void* pArgs)
{
    char                *tBuffer;
    int                 tBufferSize;
    int                 tFifoEntry = 0;
    AVFrame             *tInputFrame;
    AVFrame             *tOutputFrame;
    uint8_t             *tOutputBuffer;
    /* current chunk */
    int                 tCurrentChunkSize = 0;

    LOG(LOG_WARN, "+++++++++++++++++ VIDEO scaler thread started");

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Scaler(" + toString(mSourceResX) + "*" + toString(mSourceResY) + ")");
    int tOutputBufferSize = avpicture_get_size(mTargetPixelFormat, mTargetResX, mTargetResY) + FF_INPUT_BUFFER_PADDING_SIZE;

    // allocate chunk buffer
    tOutputBuffer = (uint8_t*)malloc(tOutputBufferSize);

    // Allocate video frame
    LOG(LOG_VERBOSE, "..allocating memory for output frame");
    if ((tOutputFrame = MediaSource::AllocFrame()) == NULL)
    {
        // acknowledge failed"
        LOG(LOG_ERROR, "Out of video memory in avcodec_alloc_frame()");
    }

    // Assign appropriate parts of buffer to image planes in frame
    avpicture_fill((AVPicture *)tOutputFrame, (uint8_t *)tOutputBuffer, mTargetPixelFormat, mTargetResX, mTargetResY);

    // Allocate video frame for format
    LOG(LOG_VERBOSE, "..allocating memory for %s input frame", mName.c_str());
    if ((tInputFrame = MediaSource::AllocFrame()) == NULL)
    {
        // acknowledge failed"
        LOG(LOG_ERROR, "Out of video memory in avcodec_alloc_frame()");
    }

    // allocate software scaler context, input/output FIFO
    LOG(LOG_VERBOSE, "..allocating %s video scaler context", mName.c_str());
    mVideoScalerContext = sws_getCachedContext(mVideoScalerContext, mSourceResX, mSourceResY, mSourcePixelFormat, mTargetResX, mTargetResY, mTargetPixelFormat, SWS_BICUBIC, NULL, NULL, NULL);
    if (mVideoScalerContext == NULL)
    {
    	LOG(LOG_ERROR, "Got invalid video scaler context");
    }

    LOG(LOG_VERBOSE, "..creating %s video scaler output FIFO", mName.c_str());
    mOutputFifo = new MediaFifo(mQueueSize, tOutputBufferSize, "VIDEO-ScalerOutput/" + mName);

    mChunkNumber = 0;
    mScalerNeeded = true;

    LOG(LOG_WARN, "================ Entering main VIDEO scaling loop");
    while(mScalerNeeded)
    {
        mScalingThreadMutex.lock();
        if (mInputFifo != NULL)
        {
			#ifdef VS_DEBUG_PACKETS
        		LOG(LOG_VERBOSE, "Waiting for new input for scaling");
			#endif
            tFifoEntry = mInputFifo->ReadFifoExclusive(&tBuffer, tBufferSize);
			#ifdef VS_DEBUG_PACKETS
	            LOG(LOG_VERBOSE, "Got new input of %d bytes for scaling", tBufferSize);
			#endif
            if (mScalerNeeded)
            {
                if (tBufferSize > 0)
                {
                    mChunkNumber++;
                    //HINT: we only get input if mStreamActivated is set and we have some registered media sinks

                    tCurrentChunkSize = 0;
                    //HINT: media type is always MEDIA_VIDEO here

                    // ####################################################################
                    // ### PREPARE INPUT FRAME
                    // ###################################################################
                    // Assign appropriate parts of buffer to image planes in tInputFrame
                    avpicture_fill((AVPicture *)tInputFrame, (uint8_t *)tBuffer, mSourcePixelFormat, mSourceResX, mSourceResY);

                    // set frame number in corresponding entries within AVFrame structure
                    tInputFrame->pts = mChunkNumber;
                    tInputFrame->coded_picture_number = mChunkNumber;
                    tInputFrame->display_picture_number = mChunkNumber;

                    #ifdef VS_DEBUG_PACKETS
                        LOG(LOG_VERBOSE, "SCALER-new input video frame..");
                        LOG(LOG_VERBOSE, "      ..key frame: %d", tInputFrame->key_frame);
                        switch(tInputFrame->pict_type)
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
                                    LOG(LOG_VERBOSE, "      ..picture type: %d", tInputFrame->pict_type);
                                    break;
                        }
                        LOG(LOG_VERBOSE, "      ..pts: %ld", tInputFrame->pts);
                        LOG(LOG_VERBOSE, "      ..coded pic number: %d", tInputFrame->coded_picture_number);
                        LOG(LOG_VERBOSE, "      ..display pic number: %d", tInputFrame->display_picture_number);
                    #endif

                    // ####################################################################
                    // ### SCALE FRAME (CONVERT)
                    // ###################################################################
                    int64_t tTime = Time::GetTimeStamp();
                    // convert

                    #ifdef VS_DEBUG_PACKETS
                        LOG(LOG_VERBOSE, "%s-scaling frame %d, source res: %d*%d (fmt: %d) to %d*%d, scaler context at %p", mName.c_str(), mChunkNumber, mSourceResX, mSourceResY, (int)mSourcePixelFormat, mTargetResX, mTargetResY, mVideoScalerContext);
                        LOG(LOG_VERBOSE, "Video input frame data: %p, %p, %p, %p", tInputFrame->data[0], tInputFrame->data[1], tInputFrame->data[2], tInputFrame->data[3]);
                        LOG(LOG_VERBOSE, "Video input frame line size: %d, %d, %d, %d", tInputFrame->linesize[0], tInputFrame->linesize[1], tInputFrame->linesize[2], tInputFrame->linesize[3]);
                        LOG(LOG_VERBOSE, "Video output frame data: %p, %p, %p, %p", tOutputFrame->data[0], tOutputFrame->data[1], tOutputFrame->data[2], tOutputFrame->data[3]);
                        LOG(LOG_VERBOSE, "Video output frame line size: %d, %d, %d, %d", tOutputFrame->linesize[0], tOutputFrame->linesize[1], tOutputFrame->linesize[2], tOutputFrame->linesize[3]);
                    #endif
                    HM_sws_scale(mVideoScalerContext, tInputFrame->data, tInputFrame->linesize, 0, mSourceResY, tOutputFrame->data, tOutputFrame->linesize);
                    #ifdef VS_DEBUG_PACKETS
                        LOG(LOG_VERBOSE, "..video scaling for %s finished", mName.c_str());
                        int64_t tTime2 = Time::GetTimeStamp();
                        LOG(LOG_VERBOSE, "SCALER-scaling video frame took %ld us", tTime2 - tTime);
                    #endif

                    // size of scaled output frame
                    tCurrentChunkSize = avpicture_get_size(mTargetPixelFormat, mTargetResX, mTargetResY);

                    #ifdef VS_DEBUG_PACKETS
                        LOG(LOG_VERBOSE, "SCALER-new output video frame..");
                        LOG(LOG_VERBOSE, "      ..key frame: %d", tOutputFrame->key_frame);
                        switch(tOutputFrame->pict_type)
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
                                    LOG(LOG_VERBOSE, "      ..picture type: %d", tOutputFrame->pict_type);
                                    break;
                        }
                        LOG(LOG_VERBOSE, "      ..pts: %ld", tOutputFrame->pts);
                        LOG(LOG_VERBOSE, "      ..coded pic number: %d", tOutputFrame->coded_picture_number);
                        LOG(LOG_VERBOSE, "      ..display pic number: %d", tOutputFrame->display_picture_number);
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

                        LOG(LOG_VERBOSE, "..forwarded the empty chunk from input to output");
                    }else
                    	LOG(LOG_ERROR, "Invalid buffer size: %d", tBufferSize);
                }
            }else
                LOG(LOG_WARN, "VIDEO scaler isn't needed anymore, going to close the thread");
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
            mScalingThreadMutex.unlock();
        }else
        {
            mScalingThreadMutex.unlock();
            LOG(LOG_VERBOSE, "Suspending the scaler thread for 10 ms");
            Suspend(10 * 1000); // check every 1/100 seconds the state of the FIFO
        }
    }

    LOG(LOG_VERBOSE, "Video scaler left thread main loop");

    mOutputFifoMutex.lock();
    delete mOutputFifo;
    mOutputFifo = NULL;
    mOutputFifoMutex.unlock();

    // free the software scaler context
    sws_freeContext(mVideoScalerContext);
    mVideoScalerContext = NULL;

    // Free the frame
    av_free(tInputFrame);

    // Free the frame
    av_free(tOutputFrame);

    // free the output buffer
    free(tOutputBuffer);

    LOG(LOG_WARN, "VIDEO scaler main loop finished ----------------");

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
