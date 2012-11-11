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
 * Purpose: Implementation of a ffmpeg based local file source
 * Author:  Thomas Volkert
 * Since:   2009-02-24
 */

#include <MediaSourceFile.h>
#include <VideoScaler.h>
#include <ProcessStatisticService.h>
#include <Logger.h>
#include <HBThread.h>
#include <HBCondition.h>

#include <algorithm>
#include <string>
#include <unistd.h>
#include <stdint.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

#define MEDIA_SOURCE_FILE_QUEUE_FOR_VIDEO                 30 // in frames (each max. 16 MB for HDTV, one entry is reserved for 0-byte signaling)
#define MEDIA_SOURCE_FILE_QUEUE_FOR_AUDIO                 128 // in audio sample blocks (each about 192 kB)

// 33 ms delay for 30 fps -> rounded to 35 ms
#define MSF_FRAME_DROP_THRESHOLD                           0 //in us, 0 deactivates frame dropping

// seeking: expected maximum GOP size, used if frames are dropped after seeking to find the next key frame close to the target frame
#define MSF_SEEK_MAX_EXPECTED_GOP_SIZE                    30 // every x frames a key frame

// above which threshold value should we execute a hard file seeking? (below this threshold we do soft seeking by adjusting RT grabbing)
#define MSF_SEEK_WAIT_THRESHOLD                          1.5 // seconds

// should we use reordered PTS values from ffmpeg video decoder?
#define MSF_USE_REORDERED_PTS                              0 // on/off

// how many bytes should be delivered towards grabbing application per request?
#define MSF_DESIRED_AUDIO_INPUT_SIZE                    2 /* 16 signed int */ * MEDIA_SOURCE_SAMPLES_PER_BUFFER /* samples */ * 2 /* channels */

///////////////////////////////////////////////////////////////////////////////

#define MEDIA_SOURCE_FILE_QUEUE         ((mMediaType == MEDIA_AUDIO) ? MEDIA_SOURCE_FILE_QUEUE_FOR_AUDIO : MEDIA_SOURCE_FILE_QUEUE_FOR_VIDEO)

///////////////////////////////////////////////////////////////////////////////

MediaSourceFile::MediaSourceFile(string pSourceFile, bool pGrabInRealTime):
    MediaSource("FILE: " + pSourceFile)
{
    mFinalPictureResX = 0;
    mFinalPictureResY = 0;
    mUseFilePTS = false;
    mSeekingTargetFrameIndex = 0;
    mRecalibrateRealTimeGrabbingAfterSeeking = false;
    mFlushBuffersAfterSeeking = false;
    mSourceType = SOURCE_FILE;
    mDesiredDevice = pSourceFile;
    mGrabInRealTime = pGrabInRealTime;
    mDecoderFifo = NULL;
    mDecoderMetaDataFifo = NULL;
    mNumberOfFrames = 0;
    mCurrentFrameIndex = 0;
    mCurrentDeviceName = mDesiredDevice;
    mPictureGrabbed = false;
}

MediaSourceFile::~MediaSourceFile()
{
    LOG(LOG_VERBOSE, "Destroying %s media source file for %s", GetMediaTypeStr().c_str(), mDesiredDevice.c_str());
    if (mMediaSourceOpened)
        CloseGrabDevice();
}

///////////////////////////////////////////////////////////////////////////////

void MediaSourceFile::getVideoDevices(VideoDevices &pVList)
{
    VideoDeviceDescriptor tDevice;

    tDevice.Name = mDesiredDevice;
    tDevice.Card = mDesiredDevice;
    tDevice.Desc = "file source: \"" + mDesiredDevice + "\"";

    pVList.push_back(tDevice);
}

void MediaSourceFile::getAudioDevices(AudioDevices &pAList)
{
    AudioDeviceDescriptor tDevice;

    tDevice.Name = mDesiredDevice;
    tDevice.Card = mDesiredDevice;
    tDevice.Desc = "file source: \"" + mDesiredDevice + "\"";
    tDevice.IoType = "Input/Output";

    pAList.push_back(tDevice);
}

void MediaSourceFile::SetFrameRate(float pFps)
{
    LOG(LOG_VERBOSE, "Calls to SetFrameRate() are ignored here, otherwise the real-time playback would delay input data for the wrong FPS value");
    LOG(LOG_VERBOSE, "Ignoring %f, keeping FPS of %f", pFps, mFrameRate);
}

bool MediaSourceFile::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    mMediaType = MEDIA_VIDEO;

    if (pFps > 29.97)
        pFps = 29.97;
    if (pFps < 5)
        pFps = 5;

    LOG(LOG_VERBOSE, "Try to open video stream from file \"%s\"..", mDesiredDevice.c_str());

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(FILE)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    if (!OpenInput(mDesiredDevice.c_str(), NULL, NULL))
    	return false;

    if (!DetectAllStreams())
    	return false;

    if (!SelectStream())
    	return false;

    if (!OpenDecoder())
    	return false;

    // do we have a picture file?
    if (mFormatContext->streams[mMediaStreamIndex]->duration > 1)
    {// video stream
    	int tResult = 0;
        if((tResult = avformat_seek_file(mFormatContext, mMediaStreamIndex, 0, 0, 0, AVSEEK_FLAG_ANY)) < 0)
        {
            LOG(LOG_WARN, "Couldn't seek to the start of video stream because \"%s\".", strerror(AVUNERROR(tResult)));
        }
    }else
    {// one single picture
        // nothing to do
    }

    // allocate software scaler context
    LOG(LOG_VERBOSE, "Going to create scaler context..");

    mCurrentFrameIndex = 0;
    mPictureGrabbed = false;
    mEOFReached = false;
    mRecalibrateRealTimeGrabbingAfterSeeking = true;
    mFlushBuffersAfterSeeking = false;
    mDecodedIFrames = 0;
    mDecodedPFrames = 0;
    mDecodedBFrames = 0;
    mSourceStartPts =  mFrameRate * mFormatContext->start_time / AV_TIME_BASE;

    MarkOpenGrabDeviceSuccessful();

    // init transcoder FIFO based for RGB32 pictures
    StartDecoder();

    return true;
}

bool MediaSourceFile::OpenAudioGrabDevice(int pSampleRate, int pChannels)
{
	int tRes = 0;

	mMediaType = MEDIA_AUDIO;
    mOutputAudioChannels = pChannels;
    mOutputAudioSampleRate = pSampleRate;
    mOutputAudioFormat = AV_SAMPLE_FMT_S16; // assume we always want signed 16 bit

    LOG(LOG_VERBOSE, "Try to open audio stream from file \"%s\"..", mDesiredDevice.c_str());

    SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(FILE)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);

    if (!OpenInput(mDesiredDevice.c_str(), NULL, NULL))
    	return false;

    mCurrentDevice = mDesiredDevice;
    mCurrentDeviceName = mDesiredDevice;

    if (!DetectAllStreams())
    	return false;

    // enumerate all audio streams and store them as possible input channels
    // find correct audio stream, depending on the desired input channel
    string tEntry;
    AVDictionaryEntry *tDictEntry;
    int tAudioStreamCount = 0;
    LOG(LOG_VERBOSE, "Probing for multiple audio input channels for device %s", mCurrentDevice.c_str());
    mInputChannels.clear();
    mMediaStreamIndex = -1;
    for (int i = 0; i < (int)mFormatContext->nb_streams; i++)
    {
        if (mFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
        	if (mFormatContext->streams[i]->codec->channels < 7)
        	{
				string tLanguage = "";
				string tTitle = "";
				AVCodec *tCodec = NULL;
				tCodec = avcodec_find_decoder(mFormatContext->streams[i]->codec->codec_id);
				tDictEntry = NULL;

				// get the language of the stream
				tDictEntry = HM_av_metadata_get(mFormatContext->streams[i]->metadata, "language", NULL);
				if (tDictEntry != NULL)
				{
					if (tDictEntry->value != NULL)
					{
						tLanguage = string(tDictEntry->value);
						std::transform(tLanguage.begin(), tLanguage.end(), tLanguage.begin(), ::toupper);
						LOG(LOG_VERBOSE, "Language found: %s", tLanguage.c_str());
					}
				}

				// get the title of the stream
				tDictEntry = HM_av_metadata_get(mFormatContext->streams[i]->metadata, "title", NULL);
				if (tDictEntry != NULL)
				{
					if (tDictEntry->value != NULL)
					{
						tTitle = string(tDictEntry->value);
						LOG(LOG_VERBOSE, "Title found: %s", tTitle.c_str());
					}
				}

				LOG(LOG_VERBOSE, "Desired audio input channel: %d, current found audio input channel: %d", mDesiredInputChannel, tAudioStreamCount);
				if(tAudioStreamCount == mDesiredInputChannel)
				{
					mMediaStreamIndex = i;
					LOG(LOG_VERBOSE, "Using audio input channel %d in stream %d for grabbing", mDesiredInputChannel, i);

					// Dump information about device file
					av_dump_format(mFormatContext, i, "MediaSourceFile(audio)", false);
				}

				tAudioStreamCount++;

				if ((tLanguage != "") || (tTitle != ""))
					tEntry = tLanguage + ": " + tTitle + " (" + toString(tCodec->name) + ", " + toString(mFormatContext->streams[i]->codec->channels) + " ch)";
				else
					tEntry = "Audio " + toString(tAudioStreamCount) + " (" + toString(tCodec->name) + ", " + toString(mFormatContext->streams[i]->codec->channels) + " ch)";
				LOG(LOG_VERBOSE, "Found audio stream: %s", tEntry.c_str());
				mInputChannels.push_back(tEntry);
        	}else
        	{
        		LOG(LOG_ERROR, "Detected unsupported audio channel setup with %d channels, will ignore this audio stream", mFormatContext->streams[i]->codec->channels);
        	}
		}
    }
    if (mMediaStreamIndex == -1)
    {
        LOG(LOG_ERROR, "Couldn't find an audio stream");
        // Close the audio file
        HM_close_input(mFormatContext);
        return false;
    }

    mCurrentInputChannel = mDesiredInputChannel;

    if (!OpenDecoder())
    	return false;

	if (!OpenFormatConverter())
		return false;
		
    // fix frame size of 0 for some audio codecs and use default caudio capture frame size instead to allow 1:1 transformation
    // old PCM implementation delivered often a frame size of 1
    // some audio codecs (excl. MP3) deliver a frame size of 0
    // ==> we use 32 as threshold to catch most of the inefficient values for the frame size
//    if (mCodecContext->frame_size < 32)
//        mCodecContext->frame_size = 1024;

    if((tRes = avformat_seek_file(mFormatContext, mMediaStreamIndex, 0, 0, 0, AVSEEK_FLAG_ANY)) < 0)
    {
        LOG(LOG_WARN, "Couldn't seek to the start of audio stream because \"%s\".", strerror(AVUNERROR(tRes)));
    }

    //set duration
    if (mFormatContext->duration != (int64_t)AV_NOPTS_VALUE)
        mNumberOfFrames = mFormatContext->duration / AV_TIME_BASE * mFrameRate;
    else
        mNumberOfFrames = 0;

    mCurrentFrameIndex = 0;
    mEOFReached = false;
    mRecalibrateRealTimeGrabbingAfterSeeking = true;
    mFlushBuffersAfterSeeking = false;
    mSourceStartPts =  mFormatContext->start_time;

    MarkOpenGrabDeviceSuccessful();

    // init decoder FIFO based for 2048 samples with 16 bit and 2 channels, more samples are never produced by a media source per grabbing cycle
    StartDecoder();

    return true;
}

bool MediaSourceFile::CloseGrabDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close %s file", GetMediaTypeStr().c_str());

    if (mMediaSourceOpened)
    {
        // make sure we can free the memory structures
        StopDecoder();

        CloseAll();

        mInputChannels.clear();

        tResult = true;
    }else
        LOG(LOG_INFO, "...%s file is already closed", GetMediaTypeStr().c_str());

    mGrabbingStopped = false;

    ResetPacketStatistic();

    return tResult;
}

int MediaSourceFile::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
	int64_t tCurrentFramePts;

	#if defined(MSF_DEBUG_PACKETS) || defined(MSF_DEBUG_DECODER_STATE)
        LOG(LOG_VERBOSE, "Going to grab new chunk");
    #endif

    if (pChunkBuffer == NULL)
    {
        // acknowledge failed"
        MarkGrabChunkFailed(GetMediaTypeStr() + " grab buffer is NULL");

        return GRAB_RES_INVALID;
    }

    bool tShouldGrabNext = false;

    do{
        tShouldGrabNext = false;

        // lock grabbing
        mGrabMutex.lock();

        if (!mMediaSourceOpened)
        {
            // unlock grabbing
            mGrabMutex.unlock();

            // acknowledge failed"
            MarkGrabChunkFailed(GetMediaTypeStr() + " source is closed");

            return GRAB_RES_INVALID;
        }

        if (mGrabbingStopped)
        {
            // unlock grabbing
            mGrabMutex.unlock();

            // acknowledge failed"
            MarkGrabChunkFailed(GetMediaTypeStr() + " source is paused");

            return GRAB_RES_INVALID;
        }

        int tAvailableFrames = (mDecoderFifo != NULL) ? mDecoderFifo->GetUsage() : 0;

        // missing input?
        if (tAvailableFrames == 0)
        {
            // EOF reached?
            if (mEOFReached)
            {
                mChunkNumber++;

                // unlock grabbing
                mGrabMutex.unlock();

                LOG(LOG_VERBOSE, "No %s frame in FIFO available and EOF marker is active", GetMediaTypeStr().c_str());

                // acknowledge "success"
                MarkGrabChunkSuccessful(mChunkNumber); // don't panic, it is only EOF

                return GRAB_RES_EOF;
            }else
            {
                if ((!mSeekingWaitForNextKeyFrame) && (!mSeekingWaitForNextKeyFramePackets))
                {// okay, we want more output from the decoder thread
                    LOG(LOG_WARN, "System too slow?, %s grabber detected a buffer underrun", GetMediaTypeStr().c_str());
                }else
                {// we have to wait until the decoder thread has new data after we triggered a seeking process

                    // nothing to complain about
                }
            }
        }

        // need more input from file but EOF is not reached?
        if (tAvailableFrames < MEDIA_SOURCE_FILE_QUEUE)
        {
            #ifdef MSF_DEBUG_DECODER_STATE
                LOG(LOG_VERBOSE, "Signal to decoder that new data is needed");
            #endif
            mDecoderMutex.lock();
            mDecoderNeedWorkCondition.SignalAll();
            mDecoderMutex.unlock();
            #ifdef MSF_DEBUG_DECODER_STATE
                LOG(LOG_VERBOSE, "Signaling to decoder done");
            #endif
        }

        // read chunk data from FIFO
        if (mDecoderFifo != NULL)
        {
        	ReadOutputBuffer((char*)pChunkBuffer, pChunkSize, tCurrentFramePts);
			#ifdef MSF_DEBUG_TIMING
        		LOG(LOG_VERBOSE, "REmaining buffered frames: %d", tAvailableFrames);
			#endif
        }else
        {// decoder thread not started
            // unlock grabbing
            mGrabMutex.unlock();

            // acknowledge "success"
            MarkGrabChunkFailed(GetMediaTypeStr() + " source is not yet ready");

            return GRAB_RES_INVALID;
        }

        // increase chunk number which will be the result of the grabbing call
        mChunkNumber++;

        // did we read an empty frame?
        if (pChunkSize == 0)
        {
            // unlock grabbing
            mGrabMutex.unlock();

            // acknowledge "success"
            MarkGrabChunkSuccessful(mChunkNumber); // don't panic, it is only EOF

            return GRAB_RES_EOF;
        }

        #if defined(MSF_DEBUG_PACKETS)
            LOG(LOG_VERBOSE, "Grabbed chunk %d of size %d from decoder FIFO", mChunkNumber, pChunkSize);
        #endif

        if ((mSeekingTargetFrameIndex != 0) && (mCurrentFrameIndex < mSeekingTargetFrameIndex))
        {
            LOG(LOG_VERBOSE, "Dropping grabbed %s frame %.2f because we are still waiting for frame %.2f", GetMediaTypeStr().c_str(), (float)mCurrentFrameIndex, mSeekingTargetFrameIndex);
            tShouldGrabNext = true;
        }

        // update current PTS value if the read frame is okay
        if (!tShouldGrabNext)
        {
			#ifdef MSF_DEBUG_PACKETS
        		LOG(LOG_VERBOSE, "Setting current frame index to %ld", tChunkDesc.Pts);
			#endif
			mCurrentFrameIndex = tCurrentFramePts;
        }

        // EOF
        if ((tCurrentFramePts - mSourceStartPts >= mNumberOfFrames) && (mNumberOfFrames != 0))
        {
            LOG(LOG_VERBOSE, "Returning EOF in %s file because PTS value %ld is bigger than or equal to maximum %.2f", GetMediaTypeStr().c_str(), tCurrentFramePts - mSourceStartPts, (float)mNumberOfFrames);
            mEOFReached = true;
            tShouldGrabNext = false;
        }

        // unlock grabbing
        mGrabMutex.unlock();

    }while (tShouldGrabNext);

    // reset seeking flag
    mSeekingTargetFrameIndex = 0;

    // #########################################
    // frame rate emulation
    // #########################################
    // inspired by "output_packet" from ffmpeg.c
    if (mGrabInRealTime)
    {
		// are we waiting for first valid frame after we seeked within the input file?
		if (mRecalibrateRealTimeGrabbingAfterSeeking)
		{
			#ifdef MSF_DEBUG_CALIBRATION
				LOG(LOG_VERBOSE, "Recalibrating RT %s grabbing after seeking in input file", GetMediaTypeStr().c_str());
			#endif

			// adapt the start pts value to the time shift once more because we maybe dropped several frames during seek process
			CalibrateRTGrabbing();

			#ifdef MSF_DEBUG_SEEKING
				LOG(LOG_VERBOSE, "Read valid %s frame %.2f after seeking in input file", GetMediaTypeStr().c_str(), (float)mCurrentFrameIndex);
			#endif

			mRecalibrateRealTimeGrabbingAfterSeeking = false;
		}

		// RT grabbing - do we have to wait?
		WaitForRTGrabbing();
    }

    // acknowledge success
    MarkGrabChunkSuccessful(mChunkNumber);

    return mChunkNumber;
}

void MediaSourceFile::StartDecoder()
{
    LOG(LOG_VERBOSE, "Starting %s decoder with FIFO", GetMediaTypeStr().c_str());

    mDecoderTargetResX = mTargetResX;
    mDecoderTargetResY = mTargetResY;

    mDecoderNeeded = false;

	// start decoder main loop
	StartThread();

	int tLoops = 0;
	while(!mDecoderNeeded)
	{
		if (tLoops % 10 == 0)
			LOG(LOG_VERBOSE, "Waiting for start of %s decoding thread, loop count: %d", GetMediaTypeStr().c_str(), ++tLoops);
		Thread::Suspend(100);
	}
}

void MediaSourceFile::StopDecoder()
{
    int tSignalingRound = 0;
    char tTmp[4];

    LOG(LOG_VERBOSE, "Stopping decoder");

    if (mDecoderFifo != NULL)
    {
        // tell transcoder thread it isn't needed anymore
        mDecoderNeeded = false;

        // wait for termination of decoder thread
        do
        {
            if(tSignalingRound > 0)
                LOG(LOG_WARN, "Signaling round %d to stop %s decoder, system has high load", tSignalingRound, GetMediaTypeStr().c_str());
            tSignalingRound++;

            // force a wake up of decoder thread
            mDecoderNeedWorkCondition.SignalAll();
        }while(!StopThread(250));
    }

    LOG(LOG_VERBOSE, "Decoder stopped");
}

void* MediaSourceFile::Run(void* pArgs)
{
    AVFrame             *tSourceFrame = NULL;
    AVPacket            tPacketStruc, *tPacket = &tPacketStruc;
    int                 tFrameFinished = 0;
    int                 tBytesDecoded = 0;
    int                 tRes = 0;
    int                 tChunkBufferSize = 0;
    uint8_t             *tChunkBuffer;
    int                 tWaitLoop;
    /* current chunk */
    int                 tCurrentChunkSize = 0;
    int64_t             tCurPacketPts = 0; // input PTS/DTS stream from the packets
    int64_t             tCurFramePts = 0; // ordered PTS/DTS stream from the decoder
    /* video scaler */
    VideoScaler         *tVideoScaler = NULL;
    /* picture as input */
    AVFrame             *tRGBFrame = NULL;
    bool                tInputIsPicture = false;
    /* audio */
    AVFifoBuffer        *tSampleFifo = NULL;

    // reset EOF marker
    mEOFReached = false;

    tInputIsPicture = InputIsPicture();

    LOG(LOG_VERBOSE, "%s-Decoding thread started", GetMediaTypeStr().c_str());
    switch(mMediaType)
    {
        case MEDIA_VIDEO:
            SVC_PROCESS_STATISTIC.AssignThreadName("Video-Decoder(FILE)");

            // Allocate video frame for YUV format
            if ((tSourceFrame = avcodec_alloc_frame()) == NULL)
                LOG(LOG_ERROR, "Out of video memory");

			if (!tInputIsPicture)
			{
				LOG(LOG_VERBOSE, "Allocating all objects for a video input");

        	    tChunkBufferSize = avpicture_get_size(mCodecContext->pix_fmt, mSourceResX, mSourceResY) + FF_INPUT_BUFFER_PADDING_SIZE;

    	        // allocate chunk buffer
	            tChunkBuffer = (uint8_t*)malloc(tChunkBufferSize);

	            // create video scaler
    	        LOG(LOG_VERBOSE, "Decoder thread starts scaler thread..");
        	    tVideoScaler = new VideoScaler("Video-Decoder(FILE)");
            	if(tVideoScaler == NULL)
                	LOG(LOG_ERROR, "Invalid video scaler instance, possible out of memory");
	            tVideoScaler->StartScaler(MEDIA_SOURCE_FILE_QUEUE, mSourceResX, mSourceResY, mCodecContext->pix_fmt, mDecoderTargetResX, mDecoderTargetResY, PIX_FMT_RGB32);

	            // set the video scaler as FIFO for the decoder
    	        mDecoderFifo = tVideoScaler;
			}else
			{// we have single frame (a picture) as input
				LOG(LOG_VERBOSE, "Allocating all objects for a picture input");

				tChunkBufferSize = avpicture_get_size(PIX_FMT_RGB32, mDecoderTargetResX, mDecoderTargetResY) + FF_INPUT_BUFFER_PADDING_SIZE;

    	        // allocate chunk buffer
	            tChunkBuffer = (uint8_t*)malloc(tChunkBufferSize);

    	        LOG(LOG_VERBOSE, "Decoder thread does not need the scaler thread because input is a picture");

                // Allocate video frame for RGB format
	            if ((tRGBFrame = avcodec_alloc_frame()) == NULL)
	                LOG(LOG_ERROR, "Out of video memory");

    	        // Assign appropriate parts of buffer to image planes in tRGBFrame
	            avpicture_fill((AVPicture *)tRGBFrame, (uint8_t *)tChunkBuffer, PIX_FMT_RGB32, mDecoderTargetResX, mDecoderTargetResY);

	            LOG(LOG_VERBOSE, "Going to create in-thread scaler context..");
	            mScalerContext = sws_getContext(mCodecContext->width, mCodecContext->height, mCodecContext->pix_fmt, mDecoderTargetResX, mDecoderTargetResY, PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);

	            mDecoderFifo = new MediaFifo(MEDIA_SOURCE_FILE_QUEUE, tChunkBufferSize, GetMediaTypeStr() + "-MediaSourceFile(Data)");
			}
			

            break;
        case MEDIA_AUDIO:
            SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Decoder(FILE)");

            tChunkBufferSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;

            // allocate chunk buffer
            tChunkBuffer = (uint8_t*)malloc(tChunkBufferSize);

            mDecoderFifo = new MediaFifo(MEDIA_SOURCE_FILE_QUEUE, tChunkBufferSize, GetMediaTypeStr() + "-MediaSourceFile(Data)");

            // init fifo buffer
            tSampleFifo = HM_av_fifo_alloc(MEDIA_SOURCE_SAMPLES_MULTI_BUFFER_SIZE * 2);

            break;
        default:
            SVC_PROCESS_STATISTIC.AssignThreadName("Decoder(FILE)");
            LOG(LOG_ERROR, "Unknown media type");
            return NULL;
            break;
    }

    mDecoderMetaDataFifo = new MediaFifo(MEDIA_SOURCE_FILE_QUEUE, sizeof(ChunkDescriptor), GetMediaTypeStr() + "-MediaSourceFile(MetaData)");

    // reset last PTS
    mDecoderLastReadPts = 0;

    mDecoderNeeded = true;

    while(mDecoderNeeded)
    {
        #ifdef MSF_DEBUG_TIMING
            LOG(LOG_VERBOSE, "%s-decoder loop", GetMediaTypeStr().c_str());
        #endif
        mDecoderMutex.lock();

        if ((mDecoderFifo != NULL) && (mDecoderFifo->GetUsage() < MEDIA_SOURCE_FILE_QUEUE - 1 /* one slot for a 0 byte signaling chunk*/) /* meta data FIFO has always the same size => hence, we don't have to check its size */)
        {

            if (mEOFReached)
                LOG(LOG_WARN, "We started %s file grabbing when EOF was already reached", GetMediaTypeStr().c_str());

            tWaitLoop = 0;

            if ((!tInputIsPicture) || (!mPictureGrabbed))
            {// we try to read packet(s) from input stream -> either the desired picture or a single frame
                // Read new packet
                // return 0 if OK, < 0 if error or end of file.
                bool tShouldReadNext;
                int tReadIteration = 0;
                do
                {
                    tReadIteration++;
                    tShouldReadNext = false;

                    if (tReadIteration > 1)
                    {
                        // free packet buffer
                        #ifdef MSF_DEBUG_PACKETS
                            LOG(LOG_WARN, "Freeing the ffmpeg packet structure..(read loop: %d)", tReadIteration);
                        #endif
                        av_free_packet(tPacket);
                    }

                    // read next sample from source - blocking
                    if ((tRes = av_read_frame(mFormatContext, tPacket)) != 0)
                    {// failed to read frame
                        #ifdef MSF_DEBUG_PACKETS
                            if (!tInputIsPicture)
                                LOG(LOG_VERBOSE, "Read bad frame %d with %d bytes from stream %d and result %s(%d)", tPacket->pts, tPacket->size, tPacket->stream_index, strerror(AVUNERROR(tRes)), tRes);
                            else
                                LOG(LOG_VERBOSE, "Read bad picture %d with %d bytes from stream %d and result %s(%d)", tPacket->pts, tPacket->size, tPacket->stream_index, strerror(AVUNERROR(tRes)), tRes);
                        #endif

                        if ((!mGrabbingStopped) && (tRes != (int)AVERROR_EOF) && (tRes != (int)AVERROR(EIO)))
                            LOG(LOG_ERROR, "Couldn't grab a %s frame because \"%s\"(%d)", GetMediaTypeStr().c_str(), strerror(AVUNERROR(tRes)), tRes);

                        if (tPacket->size == 0)
                            tShouldReadNext = true;

                        if (tRes == (int)AVERROR_EOF)
                        {
                            tCurPacketPts = mNumberOfFrames;
                            LOG(LOG_WARN, "%s-Decoder reached EOF", GetMediaTypeStr().c_str());
                            mEOFReached = true;
                        }
                        if (tRes == (int)AVERROR(EIO))
                        {
                            // acknowledge failed"
                            MarkGrabChunkFailed(GetMediaTypeStr() + " source has I/O error");

                            // signal EOF instead of I/O error
                            LOG(LOG_VERBOSE, "Returning EOF in %s file because of I/O error", GetMediaTypeStr().c_str());
                            mEOFReached = true;
                        }
                    }else
                    {// new frame was read
                        if (tPacket->stream_index == mMediaStreamIndex)
                        {
                            #ifdef MSF_DEBUG_PACKETS
                        		if (!tInputIsPicture)
                        			LOG(LOG_VERBOSE, "Read good frame %d with %d bytes from stream %d", tPacket->pts, tPacket->size, tPacket->stream_index);
                        		else
                        			LOG(LOG_VERBOSE, "Read good picture %d with %d bytes from stream %d", tPacket->pts, tPacket->size, tPacket->stream_index);
                            #endif

                            // is "presentation timestamp" stored within media file?
                            if (tPacket->dts != (int64_t)AV_NOPTS_VALUE)
                            { // DTS value
                                mUseFilePTS = false;
                                if ((tPacket->dts < mDecoderLastReadPts) && (mDecoderLastReadPts != 0) && (tPacket->dts > 16 /* ignore the first frames */))
                                {
                                    #ifdef MSF_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "%s-DTS values are non continuous in file, read DTS: %ld, last %ld, alternative PTS: %ld", GetMediaTypeStr().c_str(), tPacket->dts, mDecoderLastReadPts, tPacket->pts);
                                    #endif
                                }
                            }else
                            {// PTS value
                                mUseFilePTS = true;
                                if ((tPacket->pts < mDecoderLastReadPts) && (mDecoderLastReadPts != 0) && (tPacket->pts > 16 /* ignore the first frames */))
                                {
                                    #ifdef MSF_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "%s-PTS values are non continuous in file, %ld is lower than last %ld, alternative DTS: %ld, difference is: %ld", GetMediaTypeStr().c_str(), tPacket->pts, mDecoderLastReadPts, tPacket->dts, mDecoderLastReadPts - tPacket->pts);
                                    #endif
                                }
                            }
                            if (mUseFilePTS)
                                tCurPacketPts = tPacket->pts;
                            else
                                tCurPacketPts = tPacket->dts;

                            // for seeking: is the currently read frame close to target frame index?
                            //HINT: we need a key frame in the remaining distance to the target frame)
                            if ((mSeekingTargetFrameIndex != 0) && (tCurPacketPts < mSeekingTargetFrameIndex - MSF_SEEK_MAX_EXPECTED_GOP_SIZE))
                            {
                                #ifdef MSF_DEBUG_SEEKING
                                    LOG(LOG_VERBOSE, "Dropping %s frame %ld because we are waiting for frame %.2f", GetMediaTypeStr().c_str(), tCurPacketPts, mSeekingTargetFrameIndex);
                                #endif
                                tShouldReadNext = true;
                            }else
                            {
                                if (mRecalibrateRealTimeGrabbingAfterSeeking)
                                {
                                    // wait for next key frame packets (either i-frame or p-frame
                                    if (mSeekingWaitForNextKeyFramePackets)
                                    {
                                        if (tPacket->flags & AV_PKT_FLAG_KEY)
                                        {
                                            #ifdef MSF_DEBUG_SEEKING
                                                LOG(LOG_VERBOSE, "Read first %s key packet in packet frame number %ld with flags %d from input file after seeking", GetMediaTypeStr().c_str(), tCurPacketPts, tPacket->flags);
                                            #endif
                                            mSeekingWaitForNextKeyFramePackets = false;
                                        }else
                                        {
                                            #ifdef MSF_DEBUG_SEEKING
                                                LOG(LOG_VERBOSE, "Dropping %s frame packet %ld because we are waiting for next key frame packets after seek target frame %.2f", GetMediaTypeStr().c_str(), tCurPacketPts, mSeekingTargetFrameIndex);
                                            #endif
                                           tShouldReadNext = true;
                                        }
                                    }
                                    #ifdef MSF_DEBUG_SEEKING
                                        LOG(LOG_VERBOSE, "Read %s frame number %ld from input file after seeking", GetMediaTypeStr().c_str(), tCurPacketPts);
                                    #endif
                                }else
                                {
                                    #if defined(MSF_DEBUG_DECODER_STATE) || defined(MSF_DEBUG_PACKETS)
                                        LOG(LOG_WARN, "Read %s frame number %ld from input file, last frame was %ld", GetMediaTypeStr().c_str(), tCurPacketPts, mDecoderLastReadPts);
                                    #endif
                                }
                            }
                            mDecoderLastReadPts = tCurPacketPts;
                        }else
                        {
                            tShouldReadNext = true;
                            #ifdef MSF_DEBUG_PACKETS
                                LOG(LOG_VERBOSE, "Read frame %d of stream %d instead of desired stream %d", tPacket->pts, tPacket->stream_index, mMediaStreamIndex);
                            #endif
                        }
                    }
                }while ((tShouldReadNext) && (!mEOFReached) && (mDecoderNeeded));

                #ifdef MSF_DEBUG_PACKETS
                    if (tReadIteration > 1)
                        LOG(LOG_VERBOSE, "Needed %d read iterations to get next %s  packet from source file", tReadIteration, GetMediaTypeStr().c_str());
                    //LOG(LOG_VERBOSE, "New %s chunk with size: %d and stream index: %d", GetMediaTypeStr().c_str(), tPacket->size, tPacket->stream_index);
                #endif
            }else
            {// no packet was generated
				// generate dummy packet (av_free_packet() will destroy it later)
                av_init_packet(tPacket);
            }
            
            if (((tPacket->data != NULL) && (tPacket->size > 0)) || ((mPictureGrabbed /* we already grabbed the single frame from the picture file */) && (mMediaType == MEDIA_VIDEO)))
            {
                #ifdef MSF_DEBUG_PACKETS
                    if ((tPacket->data != NULL) && (tPacket->size > 0))
                    {
                        LOG(LOG_VERBOSE, "New %s packet..", GetMediaTypeStr().c_str());
                        LOG(LOG_VERBOSE, "      ..duration: %d", tPacket->duration);
                        LOG(LOG_VERBOSE, "      ..pts: %ld stream [%d] pts: %ld", tPacket->pts, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->pts.val);
                        LOG(LOG_VERBOSE, "      ..stream: %ld", tPacket->stream_index);
                        LOG(LOG_VERBOSE, "      ..dts: %ld", tPacket->dts);
                        LOG(LOG_VERBOSE, "      ..size: %d", tPacket->size);
                        LOG(LOG_VERBOSE, "      ..pos: %ld", tPacket->pos);
                        if (tPacket->flags == AV_PKT_FLAG_KEY)
                            LOG(LOG_VERBOSE, "      ..flags: key frame");
                        else
                            LOG(LOG_VERBOSE, "      ..flags: %d", tPacket->flags);
                    }
                #endif

                //LOG(LOG_VERBOSE, "New %s packet: dts: %ld, pts: %ld, pos: %ld, duration: %d", GetMediaTypeStr().c_str(), tPacket->dts, tPacket->pts, tPacket->pos, tPacket->duration);

                // do we have to flush buffers after seeking?
                if (mFlushBuffersAfterSeeking)
                {
                    LOG(LOG_VERBOSE, "Flushing %s codec internal buffers after seeking in input file", GetMediaTypeStr().c_str());

                    // flush ffmpeg internal buffers
                    avcodec_flush_buffers(mCodecContext);

                    #ifdef MSF_DEBUG_SEEKING
                        LOG(LOG_VERBOSE, "Read %s packet %ld of %d bytes after seeking in input file, current stream DTS is %ld", GetMediaTypeStr().c_str(), tCurPacketPts, tPacket->size, mFormatContext->streams[mMediaStreamIndex]->cur_dts);
                    #endif

                    mFlushBuffersAfterSeeking = false;
                }

                // #########################################
                // process packet
                // #########################################
                tCurrentChunkSize = 0;
                switch(mMediaType)
                {
                    case MEDIA_VIDEO:
                        {
                            // ############################
                            // ### DECODE FRAME
                            // ############################
                            if ((!tInputIsPicture) || (!mPictureGrabbed))
                            {// we try to decode packet(s) from input stream -> either the desired picture or a single frame from the stream
								// log statistics
								AnnouncePacket(tPacket->size);
								#ifdef MSF_DEBUG_PACKETS
									LOG(LOG_VERBOSE, "Decode video frame (input is picture: %d)..", tInputIsPicture);
								#endif

								// did we read the single frame of a picture?
								if ((tInputIsPicture) && (!mPictureGrabbed))
								{// store it
									LOG(LOG_VERBOSE, "Found picture packet of size %d, pts: %ld, dts: %ld and store it in the picture buffer", tPacket->size, tPacket->pts, tPacket->dts);
									mPictureGrabbed = true;
									mEOFReached = false;
								}

					            // Decode the next chunk of data
								tFrameFinished = 0;
								tBytesDecoded = HM_avcodec_decode_video(mCodecContext, tSourceFrame, &tFrameFinished, tPacket);

								// store the data planes and line sizes for later usage (for decoder loops >= 2)
								if ((tInputIsPicture) && (mPictureGrabbed))
								{
									for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
									{
										mPictureData[i] = tSourceFrame->data[i];
										mPictureLineSize[i] = tSourceFrame->linesize[i];
									}
								}

								//LOG(LOG_VERBOSE, "New %s source frame: dts: %ld, pts: %ld, pos: %ld, pic. nr.: %d", GetMediaTypeStr().c_str(), tSourceFrame->pkt_dts, tSourceFrame->pkt_pts, tSourceFrame->pkt_pos, tSourceFrame->display_picture_number);

								// MPEG2 picture repetition
								if (tSourceFrame->repeat_pict != 0)
									LOG(LOG_ERROR, "MPEG2 picture should be repeated for %d times, unsupported feature!", tSourceFrame->repeat_pict);

								#ifdef MSF_DEBUG_PACKETS
									LOG(LOG_VERBOSE, "    ..video decoding ended with result %d and %d bytes of output\n", tFrameFinished, tBytesDecoded);
								#endif

								// log lost packets: difference between currently received frame number and the number of locally processed frames
								SetLostPacketCount(tSourceFrame->coded_picture_number - mChunkNumber);

								#ifdef MSF_DEBUG_PACKETS
									LOG(LOG_VERBOSE, "Video frame %d decoded", tSourceFrame->coded_picture_number);
								#endif

								// save PTS value to deliver it later to the frame grabbing thread
								if ((tSourceFrame->pkt_dts != (int64_t)AV_NOPTS_VALUE) && (!MSF_USE_REORDERED_PTS))
								{// use DTS value from decoder
									tCurFramePts = tSourceFrame->pkt_dts;
	                                //LOG(LOG_VERBOSE, "Setting current frame PTS to %ld", tCurFramePts);
								}else if (tSourceFrame->pkt_pts != (int64_t)AV_NOPTS_VALUE)
								{// fall back to reordered PTS value
									tCurFramePts = tSourceFrame->pkt_pts;
	                                //LOG(LOG_VERBOSE, "Setting current frame PTS to %ld", tCurFramePts);
								}else
								{// fall back to packet's PTS value
									tCurFramePts = tCurPacketPts;
	                                //LOG(LOG_VERBOSE, "Setting current frame PTS to %ld", tCurFramePts);
								}

                                if ((tSourceFrame->pkt_pts != tSourceFrame->pkt_dts) && (tSourceFrame->pkt_pts != (int64_t)AV_NOPTS_VALUE) && (tSourceFrame->pkt_dts != (int64_t)AV_NOPTS_VALUE))
                                    LOG(LOG_VERBOSE, "PTS(%ld) and DTS(%ld) differ after decoding step", tSourceFrame->pkt_pts, tSourceFrame->pkt_dts);
                            }else
                            {// reuse the stored picture
								// restoring for ffmpeg the data planes and line sizes from the stored values
								#ifdef MSF_DEBUG_PACKETS
                            		LOG(LOG_VERBOSE, "Restoring the source frame's data planes and line sizes from the stored values");
								#endif
								for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
								{
									tSourceFrame->data[i] = mPictureData[i];
									tSourceFrame->linesize[i] = mPictureLineSize[i];
									tSourceFrame->pict_type = AV_PICTURE_TYPE_I;
								}

								// simulate a monotonous increasing PTS value
								tCurPacketPts++;

								// prepare the PTS value which is later delivered to the grabbing thread
								tCurFramePts = tCurPacketPts;

								// simulate a successful decoding step
								tFrameFinished = 1;
								// some value above 0
								tBytesDecoded = INT_MAX;
                            }

							// store the derived PTS value in the fields of the source frame
							tSourceFrame->pts = tCurFramePts;
							tSourceFrame->coded_picture_number = tCurFramePts;
							tSourceFrame->display_picture_number = tCurFramePts;

							// wait for next key frame packets (either i-frame or p-frame
                            if (mSeekingWaitForNextKeyFrame)
                            {// we are still waiting for the next key frame after seeking in the file
                                if (tSourceFrame->pict_type == AV_PICTURE_TYPE_I)
                                {
                                    #ifdef MSF_DEBUG_SEEKING
                                        LOG(LOG_VERBOSE, "Read first %s key frame at frame number %ld with flags %d from input file after seeking", GetMediaTypeStr().c_str(), tCurFramePts, tPacket->flags);
                                    #endif
                                    mSeekingWaitForNextKeyFrame = false;
                                }else
                                {
                                    #ifdef MSF_DEBUG_SEEKING
                                        LOG(LOG_VERBOSE, "Dropping %s frame %ld because we are waiting for next key frame after seeking", GetMediaTypeStr().c_str(), tCurFramePts);
                                    #endif
                                }

                                tCurrentChunkSize = 0;
                            }else
                            {// we are not waiting for next key frame and can proceed as usual
                                // do we have valid input from the video decoder?
                                if ((tFrameFinished != 0) && (tBytesDecoded >= 0))
                                {
                                    // ############################
                                    // ### ANNOUNCE FRAME (statistics)
                                    // ############################
                                    AnnounceFrame(tSourceFrame);

                                    // ############################
                                    // ### RECORD FRAME
                                    // ############################
                                    // re-encode the frame and write it to file
                                    if (mRecording)
                                        RecordFrame(tSourceFrame);

                                    // ############################
                                    // ### SCALE FRAME (CONVERT): is done inside a separate thread
                                    // ############################
                                    if (!tInputIsPicture)
                                    {
                                        #ifdef MSF_DEBUG_PACKETS
                                            LOG(LOG_VERBOSE, "Scale (separate thread) video frame..");
                                            LOG(LOG_VERBOSE, "Video frame data: %p, %p, %p, %p", tSourceFrame->data[0], tSourceFrame->data[1], tSourceFrame->data[2], tSourceFrame->data[3]);
                                            LOG(LOG_VERBOSE, "Video frame line size: %d, %d, %d, %d", tSourceFrame->linesize[0], tSourceFrame->linesize[1], tSourceFrame->linesize[2], tSourceFrame->linesize[3]);
                                        #endif

//                                        //LOG(LOG_VERBOSE, "New %s RGB frame: dts: %ld, pts: %ld, pos: %ld, pic. nr.: %d", GetMediaTypeStr().c_str(), tRGBFrame->pkt_dts, tRGBFrame->pkt_pts, tRGBFrame->pkt_pos, tRGBFrame->display_picture_number);

                                        if ((tRes = avpicture_layout((AVPicture*)tSourceFrame, mCodecContext->pix_fmt, mSourceResX, mSourceResY, tChunkBuffer, tChunkBufferSize)) < 0)
                                        {
                                            LOG(LOG_WARN, "Couldn't copy AVPicture/AVFrame pixel data into chunk buffer because \"%s\"(%d)", strerror(AVUNERROR(tRes)), tRes);
                                        }else
                                        {// everything is okay, we have the current frame in tChunkBuffer and can send the frame to the video scaler
                                            tCurrentChunkSize = avpicture_get_size(mCodecContext->pix_fmt, mSourceResX, mSourceResY);
                                        }
                                        #ifdef MSF_DEBUG_PACKETS
                                            LOG(LOG_VERBOSE, "New video frame..");
                                            LOG(LOG_VERBOSE, "      ..key frame: %d", tSourceFrame->key_frame);
                                            switch(tSourceFrame->pict_type)
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
                                                        LOG(LOG_VERBOSE, "      ..picture type: %d", tSourceFrame->pict_type);
                                                        break;
                                            }
                                            LOG(LOG_VERBOSE, "      ..pts: %ld", tSourceFrame->pts);
                                            LOG(LOG_VERBOSE, "      ..pkt pts: %ld", tSourceFrame->pkt_pts);
                                            LOG(LOG_VERBOSE, "      ..pkt dts: %ld", tSourceFrame->pkt_dts);
                                            LOG(LOG_VERBOSE, "      ..resolution: %d * %d", tSourceFrame->width, tSourceFrame->height);
                                            LOG(LOG_VERBOSE, "      ..coded pic number: %d", tSourceFrame->coded_picture_number);
                                            LOG(LOG_VERBOSE, "      ..display pic number: %d", tSourceFrame->display_picture_number);
                                            LOG(LOG_VERBOSE, "Resulting frame size is %d bytes", tCurrentChunkSize);
                                        #endif
                                    }else
                                    {// we decode a picture
                                    	if  ((mFinalPictureResX != mDecoderTargetResX) || (mFinalPictureResY != mDecoderTargetResY))
                                    	{
                                            #ifdef MSF_DEBUG_PACKETS
                                                LOG(LOG_VERBOSE, "Scale (within decoder thread) video frame..");
                                                LOG(LOG_VERBOSE, "Video frame data: %p, %p", tSourceFrame->data[0], tSourceFrame->data[1]);
                                                LOG(LOG_VERBOSE, "Video frame line size: %d, %d", tSourceFrame->linesize[0], tSourceFrame->linesize[1]);
                                            #endif

                                            // scale the video frame
											LOG(LOG_VERBOSE, "Scaling video input picture..");
											tRes = HM_sws_scale(mScalerContext, tSourceFrame->data, tSourceFrame->linesize, 0, mCodecContext->height, tRGBFrame->data, tRGBFrame->linesize);
                                            if (tRes == 0)
                                                LOG(LOG_ERROR, "Failed to scale the video frame");

                                            //LOG(LOG_VERBOSE, "New %s RGB frame: dts: %ld, pts: %ld, pos: %ld, pic. nr.: %d", GetMediaTypeStr().c_str(), tRGBFrame->pkt_dts, tRGBFrame->pkt_pts, tRGBFrame->pkt_pos, tRGBFrame->display_picture_number);

                                            #ifdef MSF_DEBUG_PACKETS
                                                LOG(LOG_VERBOSE, "New video frame..");
                                                LOG(LOG_VERBOSE, "      ..key frame: %d", tSourceFrame->key_frame);
                                                switch(tSourceFrame->pict_type)
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
                                                            LOG(LOG_VERBOSE, "      ..picture type: %d", tSourceFrame->pict_type);
                                                            break;
                                                }
                                                LOG(LOG_VERBOSE, "      ..pts: %ld", tSourceFrame->pts);
                                                LOG(LOG_VERBOSE, "      ..pkt pts: %ld", tSourceFrame->pkt_pts);
                                                LOG(LOG_VERBOSE, "      ..pkt dts: %ld", tSourceFrame->pkt_dts);
                                                LOG(LOG_VERBOSE, "      ..resolution: %d * %d", tSourceFrame->width, tSourceFrame->height);
                                                LOG(LOG_VERBOSE, "      ..coded pic number: %d", tSourceFrame->coded_picture_number);
                                                LOG(LOG_VERBOSE, "      ..display pic number: %d", tSourceFrame->display_picture_number);
                                                LOG(LOG_VERBOSE, "Resulting frame size is %d bytes", tCurrentChunkSize);
                                            #endif

                                            mFinalPictureResX = mDecoderTargetResX;
                                            mFinalPictureResY = mDecoderTargetResY;
                                    	}else
                                    	{// use stored RGB frame

                                    	    //HINT: tCurremtChnkSize will be updated later
        	                                //HINT: tChunkBuffer is still valid from first decoder loop                                    	
                                        }
                                        // return size of decoded frame
                                        tCurrentChunkSize = avpicture_get_size(PIX_FMT_RGB32, mDecoderTargetResX, mDecoderTargetResY);
                                    }
                                }else
                                {
                                    if(tBytesDecoded != 0)
                                    {
                                        // only print debug output if it is not "operation not permitted"
                                        //if ((tBytesDecoded < 0) && (AVUNERROR(tBytesDecoded) != EPERM))
                                        // acknowledge failed"
                                        if (tPacket->size != tBytesDecoded)
                                            LOG(LOG_WARN, "Couldn't decode video frame %ld because \"%s\"(%d), got a decoder result: %d", tCurPacketPts, strerror(AVUNERROR(tBytesDecoded)), AVUNERROR(tBytesDecoded), (tFrameFinished == 0));
                                        else
                                            LOG(LOG_WARN, "Couldn't decode video frame %ld, got a decoder result: %d", tCurPacketPts, (tFrameFinished != 0));
                                    }else
                                        LOG(LOG_VERBOSE, "Video decoder delivered no frame");

                                    tCurrentChunkSize = 0;
                                }
                            }
                        }
                        break;

                    case MEDIA_AUDIO:
                        {
                            // ############################
                            // ### DECODE FRAME
                            // ############################
                            // log statistics
                            AnnouncePacket(tPacket->size);

                            //printf("DecodeFrame..\n");
                            // Decode the next chunk of data
                            int tOutputBufferSize = tChunkBufferSize;
                            #ifdef MSF_DEBUG_PACKETS
                                LOG(LOG_VERBOSE, "Decoding audio samples into buffer of size: %d", tOutputBufferSize);
                            #endif

                            int tBytesDecoded;

                            if (mAudioResampleContext == NULL)
                            {
                                tBytesDecoded = HM_avcodec_decode_audio(mCodecContext, (int16_t *)tChunkBuffer, &tOutputBufferSize, tPacket);
                                tCurrentChunkSize = tOutputBufferSize;
                            }else
                            {// have to insert an intermediate step, which resamples the audio chunk
                                tBytesDecoded = HM_avcodec_decode_audio(mCodecContext, (int16_t *)mResampleBuffer, &tOutputBufferSize, tPacket);

                                if(tOutputBufferSize > 0)
                                {
                                    // ############################
                                    // ### RESAMPLE FRAME (CONVERT)
                                    // ############################
                                    //HINT: we always assume 16 bit samples
                                    int tResampledBytes = (2 /*16 signed char*/ * mOutputAudioChannels) * audio_resample(mAudioResampleContext, (short*)tChunkBuffer, (short*)mResampleBuffer, tOutputBufferSize / (2 * mInputAudioChannels));
                                    #ifdef MSF_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "Have resampled %d bytes of sample rate %dHz and %d channels to %d bytes of sample rate 44100Hz and 2 channels", tOutputBufferSize, mCodecContext->sample_rate, mCodecContext->channels, tResampledBytes);
                                    #endif
                                    if(tResampledBytes > 0)
                                    {
                                        tCurrentChunkSize = tResampledBytes;
                                    }else
                                    {
                                        LOG(LOG_ERROR, "Amount of resampled bytes (%d) is invalid", tResampledBytes);
                                        tCurrentChunkSize = 0;
                                    }
                                }else
                                {
                                    LOG(LOG_ERROR, "Output buffer size %d from audio decoder is invalid", tOutputBufferSize);
                                    tCurrentChunkSize = 0;
                                }
                            }

                            if (tCurrentChunkSize > 0)
                            {
                                // ############################
                                // ### WRITE FRAME TO FIFO
                                // ############################
                                // increase fifo buffer size by size of input buffer size
                                #ifdef MSM_DEBUG_PACKETS
                                    LOG(LOG_VERBOSE, "Adding %d bytes to AUDIO FIFO with size of %d bytes", tCurrentChunkSize, av_fifo_size(tSampleFifo));
                                #endif
                                if (av_fifo_realloc2(tSampleFifo, av_fifo_size(tSampleFifo) + tCurrentChunkSize) < 0)
                                {
                                    // acknowledge failed
                                    LOG(LOG_ERROR, "Reallocation of FIFO audio buffer failed");
                                }

                                //LOG(LOG_VERBOSE, "ChunkSize: %d", pChunkSize);
                                // write new samples into fifo buffer
                                av_fifo_generic_write(tSampleFifo, tChunkBuffer, tCurrentChunkSize, NULL);

                                // save PTS value to deliver it later to the frame grabbing thread
                                //LOG(LOG_VERBOSE, "Setting current frame PTS to %ld", tCurPacketPts);
                                tCurFramePts = tCurPacketPts;
//-
                                int tLoops = 0;
                                while (av_fifo_size(tSampleFifo) >= MSF_DESIRED_AUDIO_INPUT_SIZE)
                                {
                                    tLoops++;
                                    // ############################
                                    // ### READ FRAME FROM FIFO (max. 1024 samples)
                                    // ############################
                                    #ifdef MSM_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "Loop %d-Reading %d bytes from %d bytes of fifo, current PTS: %ld", tLoops, MSF_DESIRED_AUDIO_INPUT_SIZE, av_fifo_size(tSampleFifo), tCurFramePts);
                                    #endif
                                    // read sample data from the fifo buffer
                                    HM_av_fifo_generic_read(tSampleFifo, (void*)tChunkBuffer, MSF_DESIRED_AUDIO_INPUT_SIZE);
                                    tCurrentChunkSize = MSF_DESIRED_AUDIO_INPUT_SIZE;

//-
                                    // ############################
                                    // ### RECORD FRAME
                                    // ############################
                                    // re-encode the frame and write it to file
                                    if ((mRecording) && (tCurrentChunkSize > 0))
                                        RecordSamples((int16_t *)tChunkBuffer, tCurrentChunkSize);
//+
                                    // ############################
                                    // ### WRITE FRAME TO OUTPUT FIFO
                                    // ############################
                                    // add new chunk to FIFO
                                    if (tCurrentChunkSize <= mDecoderFifo->GetEntrySize())
                                    {
										#ifdef MSF_DEBUG_PACKETS
											LOG(LOG_VERBOSE, "Writing %d %s bytes at %p to FIFO with PTS %ld", tCurrentChunkSize, GetMediaTypeStr().c_str(), tChunkBuffer, tCurFramePts);
										#endif
										WriteOutputBuffer((char*)tChunkBuffer, tCurrentChunkSize, tCurFramePts);
                                        tCurFramePts++;
                                        #ifdef MSF_DEBUG_PACKETS
                                            LOG(LOG_VERBOSE, "Successful audio buffer loop");
                                        #endif
                                    }else
                                    {
                                        LOG(LOG_ERROR, "Cannot write a %s chunk of %d bytes to the FIFO with %d bytes slots", GetMediaTypeStr().c_str(),  tCurrentChunkSize, mDecoderFifo->GetEntrySize());
                                    }
                                }

                                // reset chunk size to avoid additional writes to output FIFO because we already stored all valid audio buffers in output FIFO
                                tCurrentChunkSize = 0;
                                #ifdef MSF_DEBUG_PACKETS
                                    if (tLoops > 1)
                                        LOG(LOG_VERBOSE, "Wrote %d audio packets to output FIFO");
                                #endif
//+
                            }else
                            {
                                // only print debug output if it is not "operation not permitted"
                                //if (AVUNERROR(tBytesDecoded) != EPERM)
                                // acknowledge failed"
                                if (tPacket->size != tBytesDecoded)
                                    LOG(LOG_WARN, "Couldn't decode audio samples %ld  because \"%s\"(%d)", tCurPacketPts, strerror(AVUNERROR(tBytesDecoded)), AVUNERROR(tBytesDecoded));
                                else
                                    LOG(LOG_WARN, "Couldn't decode audio samples %ld, got a decoder result: %d", tCurPacketPts, (tFrameFinished != 0));

                                tCurrentChunkSize = 0;
                            }
                        }
                        break;
                    default:
                            LOG(LOG_ERROR, "Media type unknown");
                            break;

                }

                //HINT: usually audio buffers were already written to output FIFO within the audio switch-case-branch
                // do we still have a prepared data chunk which can be delivered towards grabbing application?
                if ((tCurrentChunkSize > 0) && (!mEOFReached))
                {// no error
                    // add new chunk to FIFO
                    #ifdef MSF_DEBUG_PACKETS
                        LOG(LOG_VERBOSE, "Writing %d %s bytes at %p to FIFO with PTS %ld", tCurrentChunkSize, GetMediaTypeStr().c_str(), tChunkBuffer, tCurFramePts);
                    #endif
                    if (tCurrentChunkSize <= mDecoderFifo->GetEntrySize())
                    {
                    	WriteOutputBuffer((char*)tChunkBuffer, tCurrentChunkSize, tCurFramePts);
                        #ifdef MSF_DEBUG_PACKETS
                            LOG(LOG_VERBOSE, "Successful decoder loop");
                        #endif
                    }else
                    {
                        LOG(LOG_ERROR, "Cannot write a %s chunk of %d bytes to the FIFO with %d bytes slots", GetMediaTypeStr().c_str(),  tCurrentChunkSize, mDecoderFifo->GetEntrySize());
                    }
                }
            }

            // free packet buffer
            av_free_packet(tPacket);

            if (mEOFReached)
            {// EOF, wait until restart
                // add empty chunk to FIFO
                #ifdef MSF_DEBUG_PACKETS
                    LOG(LOG_VERBOSE, "EOF reached, writing empty chunk to %s file decoder FIFO", GetMediaTypeStr().c_str());
                #endif

				char tData[4];
                WriteOutputBuffer(tData, 0, 0);

                // time to sleep
                #ifdef MSF_DEBUG_DECODER_STATE
                    LOG(LOG_VERBOSE, "EOF for %s source reached, wait some time and check again, loop %d", GetMediaTypeStr().c_str(), ++tWaitLoop);
                #endif
                mDecoderNeedWorkCondition.Reset();
                mDecoderNeedWorkCondition.Wait(&mDecoderMutex);
                mDecoderLastReadPts = 0;
                mEOFReached = false;
                #ifdef MSF_DEBUG_DECODER_STATE
                    LOG(LOG_VERBOSE, "Continuing after file decoding was restarted");
                #endif
            }
        }else
        {// decoder FIFO is full, nothing to be done
            #ifdef MSF_DEBUG_DECODER_STATE
                LOG(LOG_VERBOSE, "Nothing to do for %s decoder, FIFO has %d of %d entries, wait some time and check again, loop %d", GetMediaTypeStr().c_str(), mDecoderFifo->GetUsage(), mDecoderFifo->GetSize(), ++tWaitLoop);
            #endif
            mDecoderNeedWorkCondition.Reset();
            mDecoderNeedWorkCondition.Wait(&mDecoderMutex);
            #ifdef MSF_DEBUG_DECODER_STATE
                LOG(LOG_VERBOSE, "Continuing after new data is needed, current FIFO size is: %d of %d", mDecoderFifo->GetUsage(), mDecoderFifo->GetSize());
            #endif
        }

        mDecoderMutex.unlock();
    }

	switch(mMediaType)
    {
	    case MEDIA_VIDEO:
                if (!tInputIsPicture)
                {
                    LOG(LOG_WARN, "VIDEO decoder thread stops scaler thread..");
                    tVideoScaler->StopScaler();
                    LOG(LOG_VERBOSE, "VIDEO decoder thread stopped scaler thread");
                }else
                {
                    // Free the RGB frame
                    av_free(tRGBFrame);

                    sws_freeContext(mScalerContext);
                    mScalerContext = NULL;
                }
        
                // Free the YUV frame
                av_free(tSourceFrame);

                break;
	    case MEDIA_AUDIO:
                // free fifo buffer
                av_fifo_free(tSampleFifo);

                break;
	    default:
	            break;
    }

    free(tChunkBuffer);

    delete mDecoderFifo;
    delete mDecoderMetaDataFifo;

    mDecoderFifo = NULL;

    LOG(LOG_VERBOSE, "Decoder loop finished");

    return NULL;
}

void MediaSourceFile::DoSetVideoGrabResolution(int pResX, int pResY)
{
    LOG(LOG_VERBOSE, "Going to execute DoSetVideoGrabResolution()");

    if (mMediaSourceOpened)
    {
        if (mMediaType == MEDIA_UNKNOWN)
        {
            LOG(LOG_WARN, "Media type is still unknown when DoSetVideoGrabResolution is called, setting type to VIDEO ");
            mMediaType = MEDIA_VIDEO;
        }

        StopDecoder();
        StartDecoder();
    }
}

bool MediaSourceFile::InputIsPicture()
{
    bool tResult = false;

//    LOG(LOG_VERBOSE, "Source opened: %d", mMediaSourceOpened);
//    if ((mFormatContext != NULL) && (mFormatContext->streams[mMediaStreamIndex]))
//        LOG(LOG_VERBOSE, "Media type: %d", mFormatContext->streams[mMediaStreamIndex]->codec->codec_type);

    // do we have a picture?
    if ((mMediaSourceOpened) &&
        (mFormatContext != NULL) && (mFormatContext->streams[mMediaStreamIndex]) &&
        (mFormatContext->streams[mMediaStreamIndex]->codec->codec_type == AVMEDIA_TYPE_VIDEO) &&
        (mFormatContext->streams[mMediaStreamIndex]->duration == 1))
        tResult = true;

    return tResult;
}

bool MediaSourceFile::SupportsDecoderFrameStatistics()
{
    return (mMediaType == MEDIA_VIDEO);
}

GrabResolutions MediaSourceFile::GetSupportedVideoGrabResolutions()
{
    VideoFormatDescriptor tFormat;

    mSupportedVideoFormats.clear();

    if (mMediaType == MEDIA_VIDEO)
    {
        tFormat.Name="SQCIF";      //      128 ×  96
        tFormat.ResX = 128;
        tFormat.ResY = 96;
        //mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="QCIF";       //      176 × 144
        tFormat.ResX = 176;
        tFormat.ResY = 144;
        //mSupportedVideoFormats.push_back(tFormat);

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
        //mSupportedVideoFormats.push_back(tFormat);

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
        //mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="CIF16";      //     1408 × 1152
        tFormat.ResX = 1408;
        tFormat.ResY = 1152;
        //mSupportedVideoFormats.push_back(tFormat);

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

bool MediaSourceFile::SupportsRecording()
{
    return true;
}

bool MediaSourceFile::SupportsSeeking()
{
    return ((!InputIsPicture()) && (GetSeekEnd() > 0));
}

float MediaSourceFile::GetSeekEnd()
{
    float tResult = 0;
    if ((mFormatContext != NULL) && (mFormatContext->duration != (int64_t)AV_NOPTS_VALUE))
        tResult = ((float)mFormatContext->duration / AV_TIME_BASE);

    // result is in seconds
    return tResult;
}

bool MediaSourceFile::Seek(float pSeconds, bool pOnlyKeyFrames)
{
    int tResult = false;
    int tRes = 0;

    float tSeekEnd = GetSeekEnd();
    float tNumberOfFrames = mNumberOfFrames;

    //HINT: we need the original PTS values from the file
    // correcting PTS value by a possible start PTS value (offset)
    if (mSourceStartPts > 0)
    {
        //LOG(LOG_VERBOSE, "Correcting PTS value by offset: %.2f", (float)mSourceStartPts);
        tNumberOfFrames += mSourceStartPts;
    }

    if (pSeconds <= 0)
        pSeconds = 0;
    if (pSeconds > tSeekEnd)
        pSeconds = tSeekEnd;

    // if we have a picture as input we cannot seek but we pretend a successful seeking
    if (!SupportsSeeking())
    {
        LOG(LOG_WARN, "Seeking not supported for the file \"%s\"", mCurrentDevice.c_str());
        return true;
    }

    // lock grabbing
    mGrabMutex.lock();

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        //LOG(LOG_ERROR, "Tried to seek while %s file is closed", GetMediaTypeStr().c_str());
        return false;
    }

    double tFrameIndex;
    if (pSeconds <= 0)
        tFrameIndex = 0;
    else
        tFrameIndex = (mSourceStartPts + (double)pSeconds * mFrameRate); // later the value for mCurrentFrameIndex will be changed to the same value like tAbsoluteTimestamp
    float tTimeDiff = pSeconds - GetSeekPos();

    //LOG(LOG_VERBOSE, "Rel: %ld Abs: %ld", tRelativeTimestamp, tAbsoluteTimestamp);

    if ((tFrameIndex >= 0) && (tFrameIndex < tNumberOfFrames))
    {
        // seek only if it is necessary
        if (pSeconds != GetSeekPos())
        {
            mDecoderMutex.lock();

            if ((!mGrabInRealTime) || ((tTimeDiff > MSF_SEEK_WAIT_THRESHOLD) || (tTimeDiff < -MSF_SEEK_WAIT_THRESHOLD)))
            {
                float tTargetTimestamp = pSeconds * AV_TIME_BASE;
                if (mFormatContext->start_time != (int64_t)AV_NOPTS_VALUE)
                {
                    LOG(LOG_VERBOSE, "Seeking: format context describes an additional start offset of %ld", mFormatContext->start_time);
                    tTargetTimestamp += mFormatContext->start_time;
                }

                LOG(LOG_VERBOSE, "%s-SEEKING from %5.2f sec. (pts %.2f) to %5.2f sec. (pts %.2f, ts: %.2f), max. sec.: %.2f (pts %.2f), source start pts: %.2f", GetMediaTypeStr().c_str(), GetSeekPos(), mCurrentFrameIndex, pSeconds, tFrameIndex, tTargetTimestamp, tSeekEnd, tNumberOfFrames, mSourceStartPts);

                int tSeekFlags = (pOnlyKeyFrames ? 0 : AVSEEK_FLAG_ANY) | AVSEEK_FLAG_FRAME | (tFrameIndex < mCurrentFrameIndex ? AVSEEK_FLAG_BACKWARD : 0);
                mSeekingTargetFrameIndex = (int64_t)tFrameIndex;

                // VIDEO: trigger a seeking until next key frame
                if (mMediaType == MEDIA_VIDEO)
                {
                    mSeekingWaitForNextKeyFrame = true;
                    mSeekingWaitForNextKeyFramePackets = true;
                }

                tRes = (avformat_seek_file(mFormatContext, -1, INT64_MIN, tTargetTimestamp, INT64_MAX, 0) >= 0);
                if (tRes < 0)
                {
                    LOG(LOG_ERROR, "Error during absolute seeking in %s source file because \"%s\"", GetMediaTypeStr().c_str(), strerror(AVUNERROR(tResult)));
                    tResult = false;
                }else
                {
                    LOG(LOG_VERBOSE, "Seeking in %s file to frame index %.2f was successful, current dts is %ld", GetMediaTypeStr().c_str(), (float)tFrameIndex, mFormatContext->streams[mMediaStreamIndex]->cur_dts);
                    mCurrentFrameIndex = tFrameIndex;

                    // seeking was successful
                    tResult = true;
                }

                mDecoderFifo->ClearFifo();
                mDecoderMetaDataFifo->ClearFifo();
                mDecoderNeedWorkCondition.SignalAll();

                // trigger a avcodec_flush_buffers()
                mFlushBuffersAfterSeeking = true;

                // trigger a RT playback calibration after seeking
                mRecalibrateRealTimeGrabbingAfterSeeking = true;
            }else
            {
                LOG(LOG_VERBOSE, "WAITING for %2.2f sec., SEEK/WAIT threshold is %2.2f", tTimeDiff, MSF_SEEK_WAIT_THRESHOLD);
                LOG(LOG_VERBOSE, "%s-we are at frame %.2f and we should be at frame %.2f", GetMediaTypeStr().c_str(), (float)mCurrentFrameIndex, (float)tFrameIndex);

                // simulate a frame index
                mCurrentFrameIndex = tFrameIndex;

                // seek by adjusting the start time of RT grabbing
                mStartPtsUSecs = mStartPtsUSecs - tTimeDiff/* in us */ * 1000 * 1000;

                // everything is fine
                tResult = true;
            }

            // reset EOF marker
            if (mEOFReached)
            {
                LOG(LOG_VERBOSE, "Reseting EOF marker for %s stream", GetMediaTypeStr().c_str());
                mEOFReached = false;
            }

            mDecoderMutex.unlock();
        }else
        {
            LOG(LOG_VERBOSE, "%s-seeking in file skipped because position is already the desired one", GetMediaTypeStr().c_str());
            tResult = true;

            // trigger a RT playback calibration after seeking
            mRecalibrateRealTimeGrabbingAfterSeeking = true;
        }
    }else
        LOG(LOG_ERROR, "Seek position PTS=%.2f(%.2f) is out of range (0 - %.2f) for %s file, fps: %.2f, start offset: %.2f", (float)tFrameIndex, pSeconds, tNumberOfFrames, GetMediaTypeStr().c_str(), mFrameRate, (float)mSourceStartPts);

    // unlock grabbing
    mGrabMutex.unlock();

    return tResult;
}

bool MediaSourceFile::SeekRelative(float pSeconds, bool pOnlyKeyFrames)
{
    bool tResult = false;

    float tTargetPos = GetSeekPos() + pSeconds;
    if (tTargetPos < 0)
        tTargetPos = 0;
    if (tTargetPos > GetSeekEnd())
        tTargetPos = GetSeekEnd();

    tResult = Seek(tTargetPos, pOnlyKeyFrames);

    LOG(LOG_VERBOSE, "Seeking relative %.2f seconds to absolute position %.2f seconds resulted with state: %d", pSeconds, tTargetPos, tResult);

    return tResult;
}

float MediaSourceFile::GetSeekPos()
{
    float tResult = 0;
    float tSeekEnd = GetSeekEnd();
	double tCurrentFrameIndex = mCurrentFrameIndex;

	if (tCurrentFrameIndex > 0)
	{
        //HINT: we need the corrected PTS values and the one from the file
        // correcting PTS value by a possible start PTS value (offset)
        if (mSourceStartPts > 0)
        {
            //LOG(LOG_VERBOSE, "Correcting PTS value by offset: %.2f", (float)mSourceStartPts);
            tCurrentFrameIndex -= mSourceStartPts;
        }

        if (mNumberOfFrames > 0)
        {
            //LOG(LOG_VERBOSE, "Rel. progress: %.2f %.2f", tCurrentFrameIndex, mNumberOfFrames);
            float tRelProgress = tCurrentFrameIndex / mNumberOfFrames;
            if (tRelProgress <= 1.0)
                tResult = (tSeekEnd * tRelProgress);
            else
                tResult = tSeekEnd;
        }
	}

    return tResult;
}

bool MediaSourceFile::SupportsMultipleInputStreams()
{
    if ((mMediaType == MEDIA_AUDIO) && (mInputChannels.size() > 1))
        return true;
    else
        return false;
}

bool MediaSourceFile::SelectInputStream(int pIndex)
{
    bool tResult = true;

    LOG(LOG_WARN, "Selecting input channel: %d of %d max. channels, current is %d", pIndex, (int)mInputChannels.size(), mCurrentInputChannel);

    if (mCurrentInputChannel != pIndex)
    {
        mDesiredInputChannel = pIndex;

        float tCurPos = GetSeekPos();
        tResult = Reset();
        tResult &= Seek(tCurPos, false);
    }else
        LOG(LOG_VERBOSE, "Desired input channel %d is already selected, skipping %s source reset", pIndex, GetMediaTypeStr().c_str());

    return tResult;
}

vector<string> MediaSourceFile::GetInputStreams()
{
    vector<string> tResult;

    // lock grabbing
    mGrabMutex.lock();

    if(mMediaSourceOpened)
    {
        tResult = mInputChannels;
    }

    // unlock grabbing
    mGrabMutex.unlock();

    return tResult;
}

string MediaSourceFile::CurrentInputStream()
{
    AVCodec *tCodec;
    string tResult = "";
    vector<string>::iterator tIt;

    int tCount = 0;

    // lock grabbing
    mGrabMutex.lock();

    if(mMediaSourceOpened)
    {
        for (tIt = mInputChannels.begin(); tIt != mInputChannels.end(); tIt++)
        {
            if (tCount == mCurrentInputChannel)
            {
                tResult = (*tIt);
                break;
            }
            tCount++;
        }
    }

    // unlock grabbing
    mGrabMutex.unlock();

    return tResult;
}

void MediaSourceFile::CalibrateRTGrabbing()
{
    // adopt the stored pts value which represent the start of the media presentation in real-time useconds
    float  tRelativeFrameIndex = mCurrentFrameIndex - mSourceStartPts;
    double tRelativeTime = (int64_t)((double)1000000 * tRelativeFrameIndex / mFrameRate);
    #ifdef MSF_DEBUG_CALIBRATION
        LOG(LOG_VERBOSE, "Calibrating %s RT playback, old PTS start: %.2f", GetMediaTypeStr().c_str(), mStartPtsUSecs);
    #endif
    mStartPtsUSecs = av_gettime() - tRelativeTime;
    #ifdef MSF_DEBUG_CALIBRATION
        LOG(LOG_VERBOSE, "Calibrating %s RT playback: new PTS start: %.2f, rel. frame index: %.2f, rel. time: %.2f ms", GetMediaTypeStr().c_str(), mStartPtsUSecs, tRelativeFrameIndex, (float)(tRelativeTime / 1000));
    #endif
}

int MediaSourceFile::GetFrameBufferCounter()
{
	if (mDecoderFifo != NULL)
		return mDecoderFifo->GetUsage();
	else
		return 0;
}

int MediaSourceFile::GetFrameBufferSize()
{
	if (mDecoderFifo != NULL)
		return mDecoderFifo->GetSize();
	else
		return 0;
}

void MediaSourceFile::WaitForRTGrabbing()
{
    float tRelativePacketTimeUSecs, tRelativeRealTimeUSecs, tDiffPtsUSecs;
    float tRelativeFrameIndex = mCurrentFrameIndex - mSourceStartPts;
    double tPacketRealPtsTmp = 1000000 * (tRelativeFrameIndex) / mFrameRate; // current presentation time in us of the current packet from the file source
    tRelativePacketTimeUSecs = (int64_t)tPacketRealPtsTmp;
    tRelativeRealTimeUSecs = av_gettime() - mStartPtsUSecs;
    tDiffPtsUSecs = tRelativePacketTimeUSecs - tRelativeRealTimeUSecs;
    #ifdef MSF_DEBUG_TIMING
        LOG(LOG_VERBOSE, "%s-current relative frame index: %ld, relative time: %8lld us (Fps: %3.2f), stream start time: %6lld us, packet's relative play out time: %8lld us, time difference: %8lld us", GetMediaTypeStr().c_str(), tRelativeFrameIndex, tRelativeRealTimeUSecs, mFrameRate, mSourceStartPts, tRelativePacketTimeUSecs, tDiffPtsUSecs);
    #endif
    // adapt timing to real-time, ignore timings between -5 ms and +5 ms
    if (tDiffPtsUSecs > 1.0)
    {
        #ifdef MSF_DEBUG_TIMING
            LOG(LOG_WARN, "%s-sleeping for %.2f ms for frame %.2f", GetMediaTypeStr().c_str(), tDiffPtsUSecs / 1000, (float)mCurrentFrameIndex);
        #endif
		if (tDiffPtsUSecs < 3000000)
			Thread::Suspend(tDiffPtsUSecs);
		else
			LOG(LOG_ERROR, "Found unplausible time value of %.2f ms", tDiffPtsUSecs);
    }else
    {
        #ifdef MSF_DEBUG_TIMING
            if (tDiffPtsUSecs < -MSF_FRAME_DROP_THRESHOLD)
                LOG(LOG_VERBOSE, "System too slow?, %s-grabbing is %.2f ms too late", GetMediaTypeStr().c_str(), tDiffPtsUSecs / (-1000));
        #endif
    }
}

void MediaSourceFile::WriteOutputBuffer(char* pBuffer, int pBufferSize, int64_t pPts)
{
	if (mDecoderFifo == NULL)
		LOG(LOG_ERROR, "Invalid decoder FIFO");

	// write A/V data to output FIFO
	mDecoderFifo->WriteFifo(pBuffer, pBufferSize);

	// add meta description about current chunk to different FIFO
    struct ChunkDescriptor tChunkDesc;
    tChunkDesc.Pts = pPts;
    mDecoderMetaDataFifo->WriteFifo((char*) &tChunkDesc, sizeof(tChunkDesc));

    // update pre-buffer time value
    UpdatePreBufferTime();
}

void MediaSourceFile::ReadOutputBuffer(char *pBuffer, int &pBufferSize, int64_t &pPts)
{
	if (mDecoderFifo == NULL)
		LOG(LOG_ERROR, "Invalid decoder FIFO");

	// read A/V data from output FIFO
	mDecoderFifo->ReadFifo(pBuffer, pBufferSize);

    // read meta description about current chunk from different FIFO
    struct ChunkDescriptor tChunkDesc;
    int tChunkDescSize = sizeof(tChunkDesc);
    mDecoderMetaDataFifo->ReadFifo((char*)&tChunkDesc, tChunkDescSize);
    pPts = tChunkDesc.Pts;
    if (tChunkDescSize != sizeof(tChunkDesc))
        LOG(LOG_ERROR, "Read from FIFO a chunk with wrong size of %d bytes, expected size is %d bytes", tChunkDescSize, sizeof(tChunkDesc));

    // update pre-buffer time value
    UpdatePreBufferTime();
}

void MediaSourceFile::UpdatePreBufferTime()
{
	#ifdef MSF_DEBUG_TIMING
		LOG(LOG_VERBOSE, "Updating pre-buffer time value");
	#endif

	float tBufferSize = mDecoderFifo->GetUsage();

	switch(mMediaType)
	{
		case MEDIA_VIDEO:
			//LOG(LOG_VERBOSE, "Buffer usage after reading: %f", tBufferSize);
			mDecoderBufferTime = tBufferSize / mRealFrameRate;
			break;
		case MEDIA_AUDIO:
			mDecoderBufferTime = tBufferSize * MEDIA_SOURCE_SAMPLES_PER_BUFFER /* 1024 */ / mOutputAudioSampleRate;
			break;
		default:
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
