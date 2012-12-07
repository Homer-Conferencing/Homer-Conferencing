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

//HINT: This memory/network based media source tries to use the half of the entire frame buffer during grabbing.
//HINT: The remaining parts of the frame buffer are used to compensate situations with a high system load.

#include <MediaSourceMem.h>
#include <MediaSource.h>
#include <ProcessStatisticService.h>
#include <RTP.h>

#include <Logger.h>
#include <HBSystem.h>

#include <string>
#include <stdint.h>
#include <unistd.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

// maximum packet size, including encoded data and RTP/TS
#define MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE                          MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE

// seeking: expected maximum GOP size, used if frames are dropped after seeking to find the next key frame close to the target frame
#define MEDIA_SOURCE_MEM_SEEK_MAX_EXPECTED_GOP_SIZE                         30 // every x frames a key frame

// should we use reordered PTS values from ffmpeg video decoder?
#define MEDIA_SOURCE_MEM_USE_REORDERED_PTS                                  0 // on/off

// how much time do we want to buffer at maximum?
#define MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME                         ((System::GetTargetMachineType() != "x86") ? 6.0 : 2.0) // use less memory for 32 bit targets

#define MEDIA_SOURCE_MEM_PRE_BUFFER_TIME                                    (MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME / 2) // leave some seconds for high system load situations so that this part of the input queue can be used for compensating it

///////////////////////////////////////////////////////////////////////////////

// for debugging purposes: define threshold for dropping a video frame
#define MEDIA_SOURCE_MEM_VIDEO_FRAME_DROP_THRESHOLD                          0 // 0-deactivated, 35 ms for a 30 fps video stream

///////////////////////////////////////////////////////////////////////////////

MediaSourceMem::MediaSourceMem(string pName, bool pRtpActivated):
    MediaSource(pName), RTP()
{
    mDecoderFrameBufferTimeMax = MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME;
    mDecoderFramePreBufferTime = MEDIA_SOURCE_MEM_PRE_BUFFER_TIME;
    mDecoderTargetFrameIndex = 0;
    mDecoderRecalibrateRTGrabbingAfterSeeking = true;
    mDecoderFlushBuffersAfterSeeking = false;
    mDecoderSinglePictureGrabbed = false;
    mDecoderFifo = NULL;
    mDecoderMetaDataFifo = NULL;
    mDecoderFragmentFifo = NULL;
	mResXLastGrabbedFrame = 0;
	mResYLastGrabbedFrame = 0;
    mDecoderSinglePictureResX = 0;
    mDecoderSinglePictureResY = 0;
    mDecoderUsesPTSFromInputPackets = false;
    mGrabberCurrentFrameIndex = 0;
	mWrappingHeaderSize= 0;
    mGrabberProvidesRTGrabbing = true;
    mSourceType = SOURCE_MEMORY;
    mStreamPacketBuffer = (char*)malloc(MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE);
    mFragmentBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);
    mFragmentNumber = 0;
    mPacketStatAdditionalFragmentSize = 0;
    mOpenInputStream = false;
    mRtpActivated = pRtpActivated;
    RTPRegisterPacketStatistic(this);

    mSourceCodecId = CODEC_ID_NONE;

    mDecoderFragmentFifo = new MediaFifo(MEDIA_SOURCE_MEM_FRAGMENT_INPUT_QUEUE_SIZE_LIMIT, MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE, "MediaSourceMem");
	LOG(LOG_VERBOSE, "Listen for video/audio frames with queue of %d bytes", MEDIA_SOURCE_MEM_FRAGMENT_INPUT_QUEUE_SIZE_LIMIT * MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);
}

MediaSourceMem::~MediaSourceMem()
{
    StopGrabbing();

    if (mMediaSourceOpened)
        CloseGrabDevice();

    mDecoderFragmentFifoDestructionMutex.lock();
    if (mDecoderFragmentFifo != NULL)
    {
        delete mDecoderFragmentFifo;
        mDecoderFragmentFifo = NULL;
    }
    mDecoderFragmentFifoDestructionMutex.unlock();
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
    int tBufferSize = pBufferSize;

    #ifdef MSMEM_DEBUG_PACKETS
        LOGEX(MediaSourceMem, LOG_VERBOSE, "Got a call for GetNextPacket() with a packet buffer at %p and size of %d bytes", pBuffer, pBufferSize);
    #endif
    if (tMediaSourceMemInstance->mRtpActivated)
    {// rtp is active, fragmentation possible!
     // we have to parse every incoming network packet and create a frame packet from the network packets (fragments)
        char *tFragmentData;
        int tFragmentBufferSize;
        int tFragmentDataSize;
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
            #ifdef MSMEM_DEBUG_PACKET_RECEIVER
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
            	LOGEX(MediaSourceMem, LOG_VERBOSE, "Detected empty signaling fragment, returning zero data");
                return 0;
            }
            if (tFragmentBufferSize < 0)
            {
                LOGEX(MediaSourceMem, LOG_VERBOSE, "Received invalid fragment");
                return 0;
            }
            tFragmentDataSize = tFragmentBufferSize;
            // parse and remove the RTP header, extract the encapsulated frame fragment
            tFragmentIsOkay = tMediaSourceMemInstance->RtpParse(tFragmentData, tFragmentDataSize, tLastFragment, tFragmentIsSenderReport, tMediaSourceMemInstance->mSourceCodecId, false);

            // relay new data to registered sinks
            if(tFragmentIsOkay)
            {
                // relay the received fragment to registered sinks
                tMediaSourceMemInstance->RelayPacketToMediaSinks(tFragmentData, tFragmentDataSize);
            }

            if ((tFragmentIsOkay) && (!tFragmentIsSenderReport))
            {
                if (tBufferSize + tFragmentDataSize < pBufferSize)
                {
                    if (tFragmentDataSize > 0)
                    {
                        // copy the fragment to the final buffer
                        memcpy(tBuffer, tFragmentData, tFragmentDataSize);
                        tBuffer += tFragmentDataSize;
                        tBufferSize += tFragmentDataSize;
                    }
                    #ifdef MSMEM_DEBUG_PACKETS
                        LOGEX(MediaSourceMem, LOG_VERBOSE, "Resulting temporary buffer size: %d", tBufferSize);
                    #endif
                }else
                {
                    tLastFragment = false;
                    tBuffer = (char*)pBuffer;
                    tBufferSize = pBufferSize;
                    LOGEX(MediaSourceMem, LOG_ERROR, "Stream buffer of %d bytes too small for input, dropping received stream", pBufferSize);
                }
            }else
            {
                if (!tFragmentIsSenderReport)
                    LOGEX(MediaSourceMem, LOG_WARN, "Current RTP packet was reported as invalid by RTP parser, ignoring this data");
                else
                {
                    int tPacketCountReportedBySender = 0;
                    int tOctetCountReportedBySender = 0;
                    if (tMediaSourceMemInstance->RtcpParseSenderReport(tFragmentData, tFragmentDataSize, tMediaSourceMemInstance->mEndToEndDelay, tPacketCountReportedBySender, tOctetCountReportedBySender))
                    {
                        tMediaSourceMemInstance->mDecoderSynchPoints++;
						#ifdef MSMEM_DEBUG_SENDER_REPORTS
                    		LOGEX(MediaSourceMem, LOG_VERBOSE, "Sender reports: %d packets and %d bytes transmitted", tPacketCountReportedBySender, tOctetCountReportedBySender);
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
    if (mDecoderFragmentFifo == NULL)
    {
        return;
    }

	if (pBufferSize > 0)
	{
        // log statistics
        // mFragmentHeaderSize to add additional TCPFragmentHeader to the statistic if TCP is used, this is triggered by MediaSourceNet
        AnnouncePacket((int)pBufferSize + mPacketStatAdditionalFragmentSize);
	}
	
    if (mDecoderFragmentFifo->GetUsage() >= mDecoderFragmentFifo->GetSize() - 4)
    {
        LOG(LOG_WARN, "Decoder packet FIFO is near overload situation in WriteFragmet(), deleting all stored frames");

        // delete all stored frames: it is a better for the decoding!
        mDecoderFragmentFifo->ClearFifo();
    }

    mDecoderFragmentFifo->WriteFifo(pBuffer, pBufferSize);
}

void MediaSourceMem::ReadFragment(char *pData, int &pDataSize)
{
    if (mDecoderFragmentFifo == NULL)
    {
        return;
    }

    mDecoderFragmentFifo->ReadFifo(&pData[0], pDataSize);

    if (pDataSize > 0)
    {
        #ifdef MSMEM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Read fragment with number %5u at %p with size %5d towards decoder", (unsigned int)++mFragmentNumber, pData, pDataSize);
        #endif
    }

    // is FIFO near overload situation?
    if (mDecoderFragmentFifo->GetUsage() >= mDecoderFragmentFifo->GetSize() - 4)
    {
        LOG(LOG_WARN, "Decoder packet FIFO is near overload situation in ReadFragment(), deleting all stored frames");

        // delete all stored frames: it is a better for the decoding!
        mDecoderFragmentFifo->ClearFifo();
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

void MediaSourceMem::SetFrameRate(float pFps)
{
    LOG(LOG_VERBOSE, "Calls to SetFrameRate() are ignored here, otherwise the real-time playback would delay input data for the wrong FPS value");
    LOG(LOG_VERBOSE, "Ignoring %f, keeping FPS of %f", pFps, mFrameRate);
}

bool MediaSourceMem::SupportsRecording()
{
	return true;
}

bool MediaSourceMem::SupportsRelaying()
{
    return true;
}

bool MediaSourceMem::HasInputStreamChanged()
{
	return HasSourceChangedFromRTP();
}

void MediaSourceMem::StopGrabbing()
{
	MediaSource::StopGrabbing();

	LOG(LOG_VERBOSE, "Going to stop memory based %s source", GetMediaTypeStr().c_str());

    char tData[4];
    if (mDecoderFragmentFifo != NULL)
    {
        mDecoderFragmentFifo->WriteFifo(tData, 0);
        mDecoderFragmentFifo->WriteFifo(tData, 0);
    }

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
	int tResult = 0;

	mDecoderFragmentFifoDestructionMutex.lock();
    if (mDecoderFragmentFifo != NULL)
        tResult =  mDecoderFragmentFifo->GetUsage();
	mDecoderFragmentFifoDestructionMutex.unlock();

	return tResult;
}

int MediaSourceMem::GetFragmentBufferSize()
{
	int tResult = 0;

	mDecoderFragmentFifoDestructionMutex.lock();
    if (mDecoderFragmentFifo != NULL)
        tResult = mDecoderFragmentFifo->GetSize();
    mDecoderFragmentFifoDestructionMutex.unlock();

	return tResult;
}

int MediaSourceMem::CalculateFrameBufferSize()
{
    int tResult = 0;

    // how many frame input buffers do we want to store, depending on mDecoderBufferTimeMax
    //HINT: assume 44.1 kHz playback sample rate, 1024 samples per audio buffer
    //HINT: reserve one buffer for internal signaling
    switch(GetMediaType())
    {
        case MEDIA_AUDIO:
            tResult = rint(mDecoderFrameBufferTimeMax * mRealFrameRate /* 44100 samples per second / 1024 samples per frame */);
            break;
        case MEDIA_VIDEO:
            tResult = rint(mDecoderFrameBufferTimeMax * mRealFrameRate /* r_frame_rate.num / r_frame_rate.den from the AVStream */);
            break;
        default:
            LOG(LOG_ERROR, "Unsupported media type");
            break;
    }

    // add one entry for internal signaling purposes
    tResult++;

    return tResult;
}

void MediaSourceMem::DoSetVideoGrabResolution(int pResX, int pResY)
{
    LOG(LOG_VERBOSE, "Going to execute DoSetVideoGrabResolution()");

    if (mMediaSourceOpened)
    {
        if (mMediaType == MEDIA_UNKNOWN)
        {
            LOG(LOG_WARN, "Media type is still unknown when DoSetVideoGrabResolution is called, setting type to VIDEO ");
            mMediaType = MEDIA_VIDEO;
        }

        LOG(LOG_VERBOSE, "Stopping decoder from DoSetVideoGrabResolution()");
        StopDecoder();
        LOG(LOG_VERBOSE, "Starting decoder from DoSetVideoGrabResolution()");
        StartDecoder();
    }
    LOG(LOG_VERBOSE, "DoSetVideoGrabResolution() finished");
}

bool MediaSourceMem::SetInputStreamPreferences(std::string pStreamCodec, bool pDoReset)
{
    bool tResult = false;
    enum CodecID tStreamCodecId = GetCodecIDFromGuiName(pStreamCodec);

    if (mSourceCodecId != tStreamCodecId)
    {
        LOG(LOG_VERBOSE, "Setting new input streaming preferences");

        tResult = true;

        // set new codec
        LOG(LOG_VERBOSE, "    ..stream codec: %d => %d (%s)", mSourceCodecId, tStreamCodecId, pStreamCodec.c_str());
        mSourceCodecId = tStreamCodecId;

        if ((pDoReset) && (mMediaSourceOpened))
        {
            LOG(LOG_VERBOSE, "Do reset now...");
            Reset();
        }
    }

    return tResult;
}

int MediaSourceMem::GetFrameBufferCounter()
{
    if (mDecoderFifo != NULL)
        return mDecoderFifo->GetUsage();
    else
        return 0;
}

int MediaSourceMem::GetFrameBufferSize()
{
    if (mDecoderFifo != NULL)
        return mDecoderFifo->GetSize();
    else
        return 0;
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
    if (mSourceCodecId == CODEC_ID_H263P)
        mSourceCodecId = CODEC_ID_H263;

    // get a format description
    if (!DescribeInput(mSourceCodecId, &tFormat))
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

	if (!OpenFormatConverter())
		return false;

    // overwrite FPS by the playout FPS value
    mFrameRate = mRealFrameRate;

    MarkOpenGrabDeviceSuccessful();

    if (!mGrabbingStopped)
        StartDecoder();

    // enforce pre-buffering for the first frame
    mDecoderRecalibrateRTGrabbingAfterSeeking = (mDecoderFramePreBufferTime > 0);

    return true;
}

bool MediaSourceMem::OpenAudioGrabDevice(int pSampleRate, int pChannels)
{
    AVIOContext       	*tIoContext;
    AVInputFormat       *tFormat;

    mMediaType = MEDIA_AUDIO;
    mOutputAudioChannels = pChannels;
    mOutputAudioSampleRate = pSampleRate;
    mOutputAudioFormat = AV_SAMPLE_FMT_S16; // assume we always want signed 16 bit

    LOG(LOG_VERBOSE, "Trying to open the audio source");

    SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(MEM)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);

    // get a format description
    if (!DescribeInput(mSourceCodecId, &tFormat))
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

    // ffmpeg might have difficulties detecting the correct input format, enforce correct audio parameters
    AVCodecContext *tCodec = mFormatContext->streams[mMediaStreamIndex]->codec;
    switch(tCodec->codec_id)
    {
    	case CODEC_ID_ADPCM_G722:
    		tCodec->channels = 1;
    		tCodec->sample_rate = 16000;
			break;
    	case CODEC_ID_GSM:
    	case CODEC_ID_PCM_ALAW:
		case CODEC_ID_PCM_MULAW:
			tCodec->channels = 1;
			tCodec->sample_rate = 8000;
			break;
    	case CODEC_ID_PCM_S16BE:
    		tCodec->channels = 2;
    		tCodec->sample_rate = 44100;
			break;
		default:
			break;
    }

    // finds and opens the correct decoder
    if (!OpenDecoder())
    	return false;

    if (!OpenFormatConverter())
		return false;

    // overwrite FPS by the playout FPS value
    mFrameRate = mRealFrameRate;

    MarkOpenGrabDeviceSuccessful();

    if (!mGrabbingStopped)
        StartDecoder();

    // enforce pre-buffering for the first frame
    mDecoderRecalibrateRTGrabbingAfterSeeking = (mDecoderFramePreBufferTime > 0);

    return true;
}

bool MediaSourceMem::CloseGrabDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close %s stream", GetMediaTypeStr().c_str());

    if (mMediaSourceOpened)
    {
        // make sure we can free the memory structures
        StopDecoder();

        CloseAll();

        tResult = true;
    }else
        LOG(LOG_INFO, "...%s stream was already closed", GetMediaTypeStr().c_str());

    mGrabbingStopped = false;

    // reset the FIFO to have a clean FIFO next time we open the media source again
    if (mDecoderFragmentFifo != NULL)
        mDecoderFragmentFifo->ClearFifo();

    ResetPacketStatistic();

    mGrabberCurrentFrameIndex = 0;
    mDecoderSinglePictureGrabbed = false;
    mEOFReached = false;
    mDecoderRecalibrateRTGrabbingAfterSeeking = true;
    mDecoderFlushBuffersAfterSeeking = false;
    mResXLastGrabbedFrame = 0;
    mResYLastGrabbedFrame = 0;
    mDecoderSynchPoints = 0;

    return tResult;
}

int MediaSourceMem::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    int64_t tCurrentFramePts;

    #if defined(MSMEM_DEBUG_PACKETS) || defined(MSMEM_DEBUG_DECODER_STATE)
        LOG(LOG_VERBOSE, "Going to grab new chunk");
    #endif

    if (pChunkBuffer == NULL)
    {
        // acknowledge failed"
        MarkGrabChunkFailed(GetMediaTypeStr() + " grab buffer is NULL");

        return GRAB_RES_INVALID;
    }

    bool tShouldGrabNext = false;
    int tGrabLoops = 0;

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
                mFrameNumber++;

                // unlock grabbing
                mGrabMutex.unlock();

                LOG(LOG_VERBOSE, "No %s frame in FIFO available and EOF marker is active", GetMediaTypeStr().c_str());

                // acknowledge "success"
                MarkGrabChunkSuccessful(mFrameNumber); // don't panic, it is only EOF

                LOG(LOG_WARN, "Signaling EOF");
                return GRAB_RES_EOF;
            }else
            {
                if ((!mDecoderWaitForNextKeyFrame) && (!mDecoderWaitForNextKeyFramePackets))
                {// okay, we want more output from the decoder thread
                    if (GetSourceType() == SOURCE_FILE)
                    {// source is a file
                        LOG(LOG_WARN, "System too slow?, %s grabber detected a buffer underrun", GetMediaTypeStr().c_str());
                    }else
                    {// source is memory/network
                        if ((mDecoderFramePreBufferingAutoRestart) && (mDecoderFramePreBufferTime > 0))
                        {// time to restart pre-buffering
                            CalibrateRTGrabbing();
                        }
                    }
                }else
                {// we have to wait until the decoder thread has new data after we triggered a seeking process
                    // nothing to complain about
                }
            }
        }

        // read chunk data from FIFO
        if (mDecoderFifo != NULL)
        {
            // need more input but EOF is not reached?
            if ((tAvailableFrames < mDecoderFifo->GetSize()) && ((!mEOFReached) || (InputIsPicture())))
            {
                #ifdef MSMEM_DEBUG_DECODER_STATE
                    LOG(LOG_VERBOSE, "Signal to decoder that new data is needed");
                #endif
                mDecoderNeedWorkConditionMutex.lock();
                mDecoderNeedWorkCondition.SignalAll();
                mDecoderNeedWorkConditionMutex.unlock();
                #ifdef MSMEM_DEBUG_DECODER_STATE
                    LOG(LOG_VERBOSE, "Signaling to decoder done");
                #endif
            }

            ReadFrameOutputBuffer((char*)pChunkBuffer, pChunkSize, tCurrentFramePts);
            #ifdef MSMEM_DEBUG_PACKETS
                LOG(LOG_VERBOSE, "Remaining buffered frames in decoder FIFO: %d", tAvailableFrames);
            #endif
        }else
        {// decoder thread not started yet
            LOG(LOG_WARN, "Decoder main loop not ready yet");
            tShouldGrabNext = true;
        }

        // increase chunk number which will be the result of the grabbing call
        mFrameNumber++;

        // did we read an empty frame?
        if (pChunkSize == 0)
        {
            // unlock grabbing
            mGrabMutex.unlock();

            // acknowledge "success"
            MarkGrabChunkSuccessful(mFrameNumber); // don't panic, it is only EOF

            LOG(LOG_WARN, "Signaling invalid result");
            return GRAB_RES_INVALID;
        }

        #ifdef MSMEM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Grabbed chunk %d of size %d with pts %ldfrom decoder FIFO", mFrameNumber, pChunkSize, tCurrentFramePts);
        #endif

        if ((mDecoderTargetFrameIndex != 0) && (mGrabberCurrentFrameIndex < mDecoderTargetFrameIndex))
        {// we are waiting for some special frame number
            LOG(LOG_VERBOSE, "Dropping grabbed %s frame %.2f because we are still waiting for frame %.2f", GetMediaTypeStr().c_str(), (float)mGrabberCurrentFrameIndex, mDecoderTargetFrameIndex);
            tShouldGrabNext = true;
        }

        // update current PTS value if the read frame is okay
        if (!tShouldGrabNext)
        {
            #ifdef MSMEM_DEBUG_PACKETS
                LOG(LOG_VERBOSE, "Setting current frame index to %ld", tCurrentFramePts);
            #endif
            mGrabberCurrentFrameIndex = tCurrentFramePts;
        }

        // check for EOF
        int64_t tCurrentRelativeFramePts = tCurrentFramePts - mSourceStartPts;
        if ((tCurrentRelativeFramePts >= mNumberOfFrames) && (mNumberOfFrames != 0) && (!InputIsPicture()))
        {// PTS value is bigger than possible max. value, EOF reached
            LOG(LOG_VERBOSE, "Returning EOF for %s input because PTS value %ld (%ld - %.2f) is bigger than or equal to maximum %.2f", GetMediaTypeStr().c_str(), tCurrentRelativeFramePts, tCurrentFramePts, (float)mSourceStartPts, (float)mNumberOfFrames);
            mEOFReached = true;
            tShouldGrabNext = false;
        }

        // unlock grabbing
        mGrabMutex.unlock();

        #ifdef MSMEM_DEBUG_PACKETS
            if (tShouldGrabNext)
                LOG(LOG_VERBOSE, "Have to grab another frame, loop: %d", ++tGrabLoops);
        #endif
    }while (tShouldGrabNext);

    // reset seeking flag
    mDecoderTargetFrameIndex = 0;

    // #########################################
    // frame rate emulation
    // #########################################
    // inspired by "output_packet" from ffmpeg.c
    if (mGrabberProvidesRTGrabbing)
    {
        // are we waiting for first valid frame after we seeked within the input stream?
        if (mDecoderRecalibrateRTGrabbingAfterSeeking)
        {
            #ifdef MSMEM_DEBUG_CALIBRATION
                LOG(LOG_VERBOSE, "Recalibrating RT %s grabbing after seeking in input stream", GetMediaTypeStr().c_str());
            #endif

            // adapt the start pts value to the time shift once more because we maybe dropped several frames during seek process
            CalibrateRTGrabbing();

            #ifdef MSMEM_DEBUG_SEEKING
                LOG(LOG_VERBOSE, "Read valid %s frame %.2f after seeking in input stream", GetMediaTypeStr().c_str(), (float)mGrabberCurrentFrameIndex);
            #endif

            mDecoderRecalibrateRTGrabbingAfterSeeking = false;
        }

        // RT grabbing - do we have to wait?
        WaitForRTGrabbing();
    }

    // acknowledge success
    MarkGrabChunkSuccessful(mFrameNumber);

    return mFrameNumber;
}

bool MediaSourceMem::InputIsPicture()
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

void MediaSourceMem::StartDecoder()
{
    LOG(LOG_VERBOSE, "Starting %s decoder with FIFO", GetMediaTypeStr().c_str());

    mDecoderTargetResX = mTargetResX;
    mDecoderTargetResY = mTargetResY;

    mDecoderNeeded = false;

    if (!IsRunning())
    {
        // start decoder main loop
        StartThread();

        int tLoops = 0;

        // wait until thread is running
        while ((!IsRunning() /* wait until thread is started */) || (!mDecoderNeeded /* wait until thread has finished the init. process */))
        {
            if (tLoops % 10 == 0)
                LOG(LOG_VERBOSE, "Waiting for start of %s decoding thread, loop count: %d", GetMediaTypeStr().c_str(), ++tLoops);
            Thread::Suspend(100 * 1000);
        }
    }
}

void MediaSourceMem::StopDecoder()
{
	char tmp[4];
    int tSignalingRound = 0;

    LOG(LOG_VERBOSE, "Stopping decoder");

    mDecoderFragmentFifoDestructionMutex.lock();
    if (mDecoderFifo != NULL)
    {
        // tell decoder thread it isn't needed anymore
        mDecoderNeeded = false;

        // wait for termination of decoder thread
        do
        {
            if(tSignalingRound > 0)
                LOG(LOG_WARN, "Signaling round %d to stop %s decoder, system has high load", tSignalingRound, GetMediaTypeStr().c_str());
            tSignalingRound++;

            WriteFragment(tmp, 0);

            // force a wake up of decoder thread
            mDecoderNeedWorkCondition.SignalAll();

            Suspend(100 * 1000);
        }while(IsRunning());
    }
    mDecoderFragmentFifoDestructionMutex.unlock();

    LOG(LOG_VERBOSE, "Decoder stopped");
}

VideoScaler* MediaSourceMem::CreateVideoScaler()
{
    VideoScaler *tResult;

    LOG(LOG_VERBOSE, "Starting video scaler thread..");
    tResult = new VideoScaler("Video-Decoder(" + GetSourceTypeStr() + ")");
    if(tResult == NULL)
        LOG(LOG_ERROR, "Invalid video scaler instance, possible out of memory");
    tResult->StartScaler(CalculateFrameBufferSize(), mSourceResX, mSourceResY, mCodecContext->pix_fmt, mDecoderTargetResX, mDecoderTargetResY, PIX_FMT_RGB32);

    return tResult;
}

void MediaSourceMem::DestroyVideoScaler(VideoScaler *pScaler)
{
    pScaler->StopScaler();
}

//###################################
//### Decoder thread
//###################################
void* MediaSourceMem::Run(void* pArgs)
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

    LOG(LOG_WARN, ">>>>>>>>>>>>>>>> %s-Decoding thread for %s media source started", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());
    switch(mMediaType)
    {
        case MEDIA_VIDEO:
            SVC_PROCESS_STATISTIC.AssignThreadName("Video-Decoder(" + GetSourceTypeStr() + ")");

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
                tVideoScaler = CreateVideoScaler();

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

                mDecoderFifo = new MediaFifo(CalculateFrameBufferSize(), tChunkBufferSize, GetMediaTypeStr() + "-MediaSource" + GetSourceTypeStr() + "(Data)");
            }

            break;
        case MEDIA_AUDIO:
            SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Decoder(" + GetSourceTypeStr() + ")");

            tChunkBufferSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;

            // allocate chunk buffer
            tChunkBuffer = (uint8_t*)malloc(tChunkBufferSize);

            mDecoderFifo = new MediaFifo(CalculateFrameBufferSize(), tChunkBufferSize, GetMediaTypeStr() + "-MediaSource" + GetSourceTypeStr() + "(Data)");

            // init fifo buffer
            tSampleFifo = HM_av_fifo_alloc(MEDIA_SOURCE_SAMPLES_MULTI_BUFFER_SIZE * 2);

            break;
        default:
            SVC_PROCESS_STATISTIC.AssignThreadName("Decoder(" + GetSourceTypeStr() + ")");
            LOG(LOG_ERROR, "Unknown media type");
            return NULL;
            break;
    }

    mDecoderMetaDataFifo = new MediaFifo(CalculateFrameBufferSize(), sizeof(ChunkDescriptor), GetMediaTypeStr() + "-MediaSource" + GetSourceTypeStr() + "(MetaData)");

    // reset last PTS
    mDecoderLastReadPts = 0;

    // signal that decoder thread has finished init.
    mDecoderNeeded = true;

    LOG(LOG_WARN, "================ Entering main %s decoding loop for %s media source", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());

    while(mDecoderNeeded)
    {
        #ifdef MSMEM_DEBUG_DECODER_STATE
            LOG(LOG_VERBOSE, "%s-decoder loop", GetMediaTypeStr().c_str());
        #endif

        mDecoderNeedWorkConditionMutex.lock();
        if (!DecoderFifoFull())
        {
            mDecoderNeedWorkConditionMutex.unlock();

            if (mEOFReached)
                LOG(LOG_WARN, "We started %s grabbing when EOF was already reached", GetMediaTypeStr().c_str());

            tWaitLoop = 0;

            if ((!tInputIsPicture) || (!mDecoderSinglePictureGrabbed))
            {// we try to read packet(s) from input stream -> either the desired picture or a single frame
                // #########################################
                // read new packet
                // #########################################
                bool tShouldReadNext;
                int tReadIteration = 0;
                do
                {
                    tReadIteration++;
                    tShouldReadNext = false;

                    if (tReadIteration > 1)
                    {
                        // free packet buffer
                        #ifdef MSMEM_DEBUG_PACKETS
                            LOG(LOG_WARN, "Freeing the ffmpeg packet structure..(read loop: %d)", tReadIteration);
                        #endif
                        av_free_packet(tPacket);
                    }

                    // read next sample from source - blocking
                    if ((tRes = av_read_frame(mFormatContext, tPacket)) != 0)
                    {// failed to read frame
                        #ifdef MSMEM_DEBUG_PACKETS
                            if (!tInputIsPicture)
                                LOG(LOG_VERBOSE, "Read bad frame %d with %d bytes from stream %d and result %s(%d)", tPacket->pts, tPacket->size, tPacket->stream_index, strerror(AVUNERROR(tRes)), tRes);
                            else
                                LOG(LOG_VERBOSE, "Read bad picture %d with %d bytes from stream %d and result %s(%d)", tPacket->pts, tPacket->size, tPacket->stream_index, strerror(AVUNERROR(tRes)), tRes);
                        #endif

                        // error reporting
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
                            LOG(LOG_VERBOSE, "Returning EOF in %s stream because of I/O error", GetMediaTypeStr().c_str());
                            mEOFReached = true;
                        }
                    }else
                    {// new frame was read
                        if (tPacket->stream_index == mMediaStreamIndex)
                        {
                            #ifdef MSMEM_DEBUG_PACKETS
                                if (!tInputIsPicture)
                                    LOG(LOG_VERBOSE, "Read good frame %d with %d bytes from stream %d", tPacket->pts, tPacket->size, tPacket->stream_index);
                                else
                                    LOG(LOG_VERBOSE, "Read good picture %d with %d bytes from stream %d", tPacket->pts, tPacket->size, tPacket->stream_index);
                            #endif

                            // is "presentation timestamp" stored within media stream?
                            if (tPacket->dts != (int64_t)AV_NOPTS_VALUE)
                            { // DTS value
                                mDecoderUsesPTSFromInputPackets = false;
                                if ((tPacket->dts < mDecoderLastReadPts) && (mDecoderLastReadPts != 0) && (tPacket->dts > 16 /* ignore the first frames */))
                                {
                                    #ifdef MSMEM_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "%s-DTS values are non continuous in stream, read DTS: %ld, last %ld, alternative PTS: %ld", GetMediaTypeStr().c_str(), tPacket->dts, mDecoderLastReadPts, tPacket->pts);
                                    #endif
                                }
                            }else
                            {// PTS value
                                mDecoderUsesPTSFromInputPackets = true;
                                if ((tPacket->pts < mDecoderLastReadPts) && (mDecoderLastReadPts != 0) && (tPacket->pts > 16 /* ignore the first frames */))
                                {
                                    #ifdef MSMEM_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "%s-PTS values are non continuous in stream, %ld is lower than last %ld, alternative DTS: %ld, difference is: %ld", GetMediaTypeStr().c_str(), tPacket->pts, mDecoderLastReadPts, tPacket->dts, mDecoderLastReadPts - tPacket->pts);
                                    #endif
                                }
                            }

                            // derive the correct PTS value either from RTP data or from FPS emulation
                            if (!mRtpActivated)
                            {// use the PTS/DTS value from the stream packet
                                if (mDecoderUsesPTSFromInputPackets)
                                    tCurPacketPts = tPacket->pts;
                                else
                                    tCurPacketPts = tPacket->dts;
                            }else
                            {// derive the frame number from the RTP timestamp, which works independent from packet loss
                                tCurPacketPts = CalculateFrameNumberFromRTP();
                                #ifdef MSMEM_DEBUG_PRE_BUFFERING
                                    LOG(LOG_VERBOSE, "Got from RTP data the frame number: %d", tCurPacketPts);
                                #endif
                            }

                            // for seeking: is the currently read frame close to target frame index?
                            //HINT: we need a key frame in the remaining distance to the target frame)
                            if ((mDecoderTargetFrameIndex != 0) && (tCurPacketPts < mDecoderTargetFrameIndex - MEDIA_SOURCE_MEM_SEEK_MAX_EXPECTED_GOP_SIZE))
                            {
                                #ifdef MSMEM_DEBUG_SEEKING
                                    LOG(LOG_VERBOSE, "Dropping %s frame %ld because we are waiting for frame %.2f", GetMediaTypeStr().c_str(), tCurPacketPts, mDecoderTargetFrameIndex);
                                #endif
                                tShouldReadNext = true;
                            }else
                            {
                                if (mDecoderRecalibrateRTGrabbingAfterSeeking)
                                {
                                    // wait for next key frame packets (either i-frame or p-frame
                                    if (mDecoderWaitForNextKeyFramePackets)
                                    {
                                        if (tPacket->flags & AV_PKT_FLAG_KEY)
                                        {
                                            #ifdef MSMEM_DEBUG_SEEKING
                                                LOG(LOG_VERBOSE, "Read first %s key packet in packet frame number %ld with flags %d from input stream after seeking", GetMediaTypeStr().c_str(), tCurPacketPts, tPacket->flags);
                                            #endif
                                            mDecoderWaitForNextKeyFramePackets = false;
                                        }else
                                        {
                                            #ifdef MSMEM_DEBUG_SEEKING
                                                LOG(LOG_VERBOSE, "Dropping %s frame packet %ld because we are waiting for next key frame packets after seek target frame %.2f", GetMediaTypeStr().c_str(), tCurPacketPts, mDecoderTargetFrameIndex);
                                            #endif
                                           tShouldReadNext = true;
                                        }
                                    }
                                    #ifdef MSMEM_DEBUG_SEEKING
                                        LOG(LOG_VERBOSE, "Read %s frame number %ld from input stream after seeking", GetMediaTypeStr().c_str(), tCurPacketPts);
                                    #endif
                                }else
                                {
                                    #if defined(MSMEM_DEBUG_DECODER_STATE) || defined(MSMEM_DEBUG_PACKETS)
                                        LOG(LOG_WARN, "Read %s frame number %ld from input stream, last frame was %ld", GetMediaTypeStr().c_str(), tCurPacketPts, mDecoderLastReadPts);
                                    #endif
                                }
                            }
                            // check if PTS value is continuous
                            if (tCurPacketPts < mDecoderLastReadPts)
                            {// current packet is older than the last packet
                                LOG(LOG_WARN, "Found interleaved %s packets in %s source, non continuous PTS: %ld => %d", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), mDecoderLastReadPts, tCurPacketPts);
                            }
                            mDecoderLastReadPts = tCurPacketPts;
                        }else
                        {
                            tShouldReadNext = true;
                            #ifdef MSMEM_DEBUG_PACKETS
                                LOG(LOG_VERBOSE, "Read frame %d of stream %d instead of desired stream %d", tPacket->pts, tPacket->stream_index, mMediaStreamIndex);
                            #endif
                        }
                    }
                }while ((tShouldReadNext) && (!mEOFReached) && (mDecoderNeeded));

                #ifdef MSMEM_DEBUG_PACKETS
                    if (tReadIteration > 1)
                        LOG(LOG_VERBOSE, "Needed %d read iterations to get next %s  packet from source stream", tReadIteration, GetMediaTypeStr().c_str());
                    //LOG(LOG_VERBOSE, "New %s chunk with size: %d and stream index: %d", GetMediaTypeStr().c_str(), tPacket->size, tPacket->stream_index);
                #endif
            }else
            {// no packet was generated
                // generate dummy packet (av_free_packet() will destroy it later)
                av_init_packet(tPacket);
            }

            // #########################################
            // start packet processing
            // #########################################
            if (((tPacket->data != NULL) && (tPacket->size > 0)) || ((mDecoderSinglePictureGrabbed /* we already grabbed the single frame from the picture input */) && (mMediaType == MEDIA_VIDEO)))
            {
                #ifdef MSMEM_DEBUG_PACKET_RECEIVER
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
                if (mDecoderFlushBuffersAfterSeeking)
                {
                    LOG(LOG_VERBOSE, "Flushing %s codec internal buffers after seeking in input stream", GetMediaTypeStr().c_str());

                    // flush ffmpeg internal buffers
                    avcodec_flush_buffers(mCodecContext);

                    #ifdef MSMEM_DEBUG_SEEKING
                        LOG(LOG_VERBOSE, "Read %s packet %ld of %d bytes after seeking in input stream, current stream DTS is %ld", GetMediaTypeStr().c_str(), tCurPacketPts, tPacket->size, mFormatContext->streams[mMediaStreamIndex]->cur_dts);
                    #endif

                    mDecoderFlushBuffersAfterSeeking = false;
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
                            if ((!tInputIsPicture) || (!mDecoderSinglePictureGrabbed))
                            {// we try to decode packet(s) from input stream -> either the desired picture or a single frame from the stream
                                // log statistics
                                AnnouncePacket(tPacket->size);
                                #ifdef MSMEM_DEBUG_PACKETS
                                    LOG(LOG_VERBOSE, "Decode video frame (input is picture: %d)..", tInputIsPicture);
                                #endif

                                // did we read the single frame of a picture?
                                if ((tInputIsPicture) && (!mDecoderSinglePictureGrabbed))
                                {// store it
                                    LOG(LOG_VERBOSE, "Found picture packet of size %d, pts: %ld, dts: %ld and store it in the picture buffer", tPacket->size, tPacket->pts, tPacket->dts);
                                    mDecoderSinglePictureGrabbed = true;
                                    mEOFReached = false;
                                }

                                // Decode the next chunk of data
                                tFrameFinished = 0;
                                tBytesDecoded = HM_avcodec_decode_video(mCodecContext, tSourceFrame, &tFrameFinished, tPacket);

                                #ifdef MSMEM_DEBUG_VIDEO_FRAME_RECEIVER
                                    LOG(LOG_VERBOSE, "New video frame before PTS adaption..");
                                    LOG(LOG_VERBOSE, "      ..key frame: %d", tSourceFrame->key_frame);
                                    LOG(LOG_VERBOSE, "      ..picture type: %s-frame", GetFrameType(tSourceFrame).c_str());
                                    LOG(LOG_VERBOSE, "      ..pts: %ld", tSourceFrame->pts);
                                    LOG(LOG_VERBOSE, "      ..pkt pts: %ld", tSourceFrame->pkt_pts);
                                    LOG(LOG_VERBOSE, "      ..pkt dts: %ld", tSourceFrame->pkt_dts);
                                    LOG(LOG_VERBOSE, "      ..resolution: %d * %d", tSourceFrame->width, tSourceFrame->height);
                                    LOG(LOG_VERBOSE, "      ..coded pic number: %d", tSourceFrame->coded_picture_number);
                                    LOG(LOG_VERBOSE, "      ..display pic number: %d", tSourceFrame->display_picture_number);
                                #endif

                                // store the data planes and line sizes for later usage (for decoder loops >= 2)
                                if ((tInputIsPicture) && (mDecoderSinglePictureGrabbed))
                                {
                                    for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
                                    {
                                        mDecoderSinglePictureData[i] = tSourceFrame->data[i];
                                        mDecoderSinglePictureLineSize[i] = tSourceFrame->linesize[i];
                                    }
                                }

                                //LOG(LOG_VERBOSE, "New %s source frame: dts: %ld, pts: %ld, pos: %ld, pic. nr.: %d", GetMediaTypeStr().c_str(), tSourceFrame->pkt_dts, tSourceFrame->pkt_pts, tSourceFrame->pkt_pos, tSourceFrame->display_picture_number);

                                // MPEG2 picture repetition
                                if (tSourceFrame->repeat_pict != 0)
                                    LOG(LOG_ERROR, "MPEG2 picture should be repeated for %d times, unsupported feature!", tSourceFrame->repeat_pict);

                                #ifdef MSMEM_DEBUG_PACKETS
                                    LOG(LOG_VERBOSE, "    ..video decoding ended with result %d and %d bytes of output\n", tFrameFinished, tBytesDecoded);
                                #endif

                                // log lost packets: difference between currently received frame number and the number of locally processed frames
                                SetLostPacketCount(tSourceFrame->coded_picture_number - mFrameNumber);

                                #ifdef MSMEM_DEBUG_PACKETS
                                    LOG(LOG_VERBOSE, "Video frame %d decoded", tSourceFrame->coded_picture_number);
                                #endif

                                // do we have a video codec change at sender side?
                                if ((mSourceCodecId != 0) && (mSourceCodecId != mCodecContext->codec_id))
                                {
                                	if ((mCodecContext->codec_id == 5 /* h263 */) && (mSourceCodecId == 20 /* h263+ */))
                                	{// changed from h263+ to h263
                                		// no difference during decoding
                                	}else
                                	{// unsupported code change
										LOG(LOG_WARN, "Incoming video stream in %s source changed codec from %s(%d) to %s(%d)", GetSourceTypeStr().c_str(), GetFormatName(mSourceCodecId).c_str(), mSourceCodecId, GetFormatName(mCodecContext->codec_id).c_str(), mCodecContext->codec_id);
										LOG(LOG_ERROR, "Unsupported video codec change");
                                	}
									mSourceCodecId = mCodecContext->codec_id;
                                }

                                // check if video resolution has changed within input stream (at remode side for network streams!)
                                if ((mResXLastGrabbedFrame != mCodecContext->width) || (mResYLastGrabbedFrame != mCodecContext->height))
                                {
                                    if ((mSourceResX != mCodecContext->width) || (mSourceResY != mCodecContext->height))
                                    {
                                        LOG(LOG_WARN, "Video resolution change in input stream from %s source detected", GetSourceTypeStr().c_str());

                                        // set grabbing resolution to the resolution from the codec, which has automatically detected the new resolution
                                        mSourceResX = mCodecContext->width;
                                        mSourceResY = mCodecContext->height;

                                        // let the video scaler update the (ffmpeg based) scaler context
                                        tVideoScaler->ChangeInputResolution(mSourceResX, mSourceResY);

                                        // free the old chunk buffer
                                        free(tChunkBuffer);

                                        // calculate a new chunk buffer size for the new source video resolution
                                        tChunkBufferSize = avpicture_get_size(mCodecContext->pix_fmt, mSourceResX, mSourceResY) + FF_INPUT_BUFFER_PADDING_SIZE;

                                        // allocate the new chunk buffer
                                        tChunkBuffer = (uint8_t*)malloc(tChunkBufferSize);

                                        LOG(LOG_INFO, "Video resolution changed to %d * %d", mCodecContext->width, mCodecContext->height);
                                    }

                                    mResXLastGrabbedFrame = mCodecContext->width;
                                    mResYLastGrabbedFrame = mCodecContext->height;
                                }

                                if (!mRtpActivated)
                                {
                                    // save PTS value to deliver it later to the frame grabbing thread
                                    if ((tSourceFrame->pkt_dts != (int64_t)AV_NOPTS_VALUE) && (!MEDIA_SOURCE_MEM_USE_REORDERED_PTS))
                                    {// use DTS value from decoder
                                        #ifdef MSMEM_DEBUG_TIMING
                                            LOG(LOG_VERBOSE, "Setting current frame PTS to frame packet DTS %ld", tSourceFrame->pkt_dts);
                                        #endif
                                        tCurFramePts = tSourceFrame->pkt_dts;
                                    }else if (tSourceFrame->pkt_pts != (int64_t)AV_NOPTS_VALUE)
                                    {// fall back to reordered PTS value
                                        #ifdef MSMEM_DEBUG_TIMING
                                            LOG(LOG_VERBOSE, "Setting current frame PTS to frame packet PTS %ld", tSourceFrame->pkt_pts);
                                        #endif
                                        tCurFramePts = tSourceFrame->pkt_pts;
                                    }else
                                    {// fall back to packet's PTS value
                                        #ifdef MSMEM_DEBUG_TIMING
                                            LOG(LOG_VERBOSE, "Setting current frame PTS to packet PTS %ld", tCurPacketPts);
                                        #endif
                                        tCurFramePts = tCurPacketPts;
                                    }
                                }else
                                {// RTP active
                                    #ifdef MSMEM_DEBUG_PRE_BUFFERING
                                        LOG(LOG_VERBOSE, "Setting current frame PTS to packet PTS (derived from RTP timestamp): %ld", tCurPacketPts);
                                    #endif

                                    // use the frame numbers which were derived from RTP timestamps
                                    tCurFramePts = tCurPacketPts;
                                }

                                if ((tSourceFrame->pkt_pts != tSourceFrame->pkt_dts) && (tSourceFrame->pkt_pts != (int64_t)AV_NOPTS_VALUE) && (tSourceFrame->pkt_dts != (int64_t)AV_NOPTS_VALUE))
                                    LOG(LOG_VERBOSE, "PTS(%ld) and DTS(%ld) differ after %s decoding step", tSourceFrame->pkt_pts, tSourceFrame->pkt_dts, GetMediaTypeStr().c_str());
                            }else
                            {// reuse the stored picture
                                // restoring for ffmpeg the data planes and line sizes from the stored values
                                #ifdef MSMEM_DEBUG_PACKETS
                                    LOG(LOG_VERBOSE, "Restoring the source frame's data planes and line sizes from the stored values");
                                #endif
                                for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
                                {
                                    tSourceFrame->data[i] = mDecoderSinglePictureData[i];
                                    tSourceFrame->linesize[i] = mDecoderSinglePictureLineSize[i];
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

                            #ifdef MSMEM_DEBUG_VIDEO_FRAME_RECEIVER
                                LOG(LOG_VERBOSE, "New video frame..");
                                LOG(LOG_VERBOSE, "      ..key frame: %d", tSourceFrame->key_frame);
                                LOG(LOG_VERBOSE, "      ..picture type: %s-frame", GetFrameType(tSourceFrame).c_str());
                                LOG(LOG_VERBOSE, "      ..pts: %ld", tSourceFrame->pts);
                                LOG(LOG_VERBOSE, "      ..pkt pts: %ld", tSourceFrame->pkt_pts);
                                LOG(LOG_VERBOSE, "      ..pkt dts: %ld", tSourceFrame->pkt_dts);
                                LOG(LOG_VERBOSE, "      ..resolution: %d * %d", tSourceFrame->width, tSourceFrame->height);
                                LOG(LOG_VERBOSE, "      ..coded pic number: %d", tSourceFrame->coded_picture_number);
                                LOG(LOG_VERBOSE, "      ..display pic number: %d", tSourceFrame->display_picture_number);
                            #endif

                                                                        // wait for next key frame packets (either i-frame or p-frame
                            if (mDecoderWaitForNextKeyFrame)
                            {// we are still waiting for the next key frame after seeking in the input stream
                                if (tSourceFrame->pict_type == AV_PICTURE_TYPE_I)
                                {
                                    #ifdef MSMEM_DEBUG_SEEKING
                                        LOG(LOG_VERBOSE, "Read first %s key frame at frame number %ld with flags %d from input stream after seeking", GetMediaTypeStr().c_str(), tCurFramePts, tPacket->flags);
                                    #endif
                                    mDecoderWaitForNextKeyFrame = false;
                                }else
                                {
                                    #ifdef MSMEM_DEBUG_SEEKING
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
                                    {// we decode one frame of a stream
                                        #ifdef MSMEM_DEBUG_PACKETS
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
                                    }else
                                    {// we decode a picture
                                        if  ((mDecoderSinglePictureResX != mDecoderTargetResX) || (mDecoderSinglePictureResY != mDecoderTargetResY))
                                        {
                                            #ifdef MSMEM_DEBUG_PACKETS
                                                LOG(LOG_VERBOSE, "Scale (within decoder thread) video frame..");
                                                LOG(LOG_VERBOSE, "Video frame data: %p, %p", tSourceFrame->data[0], tSourceFrame->data[1]);
                                                LOG(LOG_VERBOSE, "Video frame line size: %d, %d", tSourceFrame->linesize[0], tSourceFrame->linesize[1]);
                                            #endif

                                            // scale the video frame
                                            LOG(LOG_VERBOSE, "Scaling video input picture..");
                                            tRes = HM_sws_scale(mScalerContext, tSourceFrame->data, tSourceFrame->linesize, 0, mCodecContext->height, tRGBFrame->data, tRGBFrame->linesize);
                                            if (tRes == 0)
                                                LOG(LOG_ERROR, "Failed to scale the video frame");

                                            LOG(LOG_VERBOSE, "Decoded picture into RGB frame: dts: %ld, pts: %ld, pos: %ld, pic. nr.: %d", tRGBFrame->pkt_dts, tRGBFrame->pkt_pts, tRGBFrame->pkt_pos, tRGBFrame->display_picture_number);

                                            mDecoderSinglePictureResX = mDecoderTargetResX;
                                            mDecoderSinglePictureResY = mDecoderTargetResY;
                                        }else
                                        {// use stored RGB frame

                                            //HINT: tCurremtChnkSize will be updated later
                                            //HINT: tChunkBuffer is still valid from first decoder loop
                                        }
                                        // return size of decoded frame
                                        tCurrentChunkSize = avpicture_get_size(PIX_FMT_RGB32, mDecoderTargetResX, mDecoderTargetResY);
                                    }
                                    #ifdef MSMEM_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "Resulting frame size is %d bytes", tCurrentChunkSize);
                                    #endif
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

                            int tOutoutAudioBytesPrSample = av_get_bytes_per_sample(mOutputAudioFormat);
                            int tInputAudioBytesPrSample = av_get_bytes_per_sample(mInputAudioFormat);

                            //printf("DecodeFrame..\n");
                            // Decode the next chunk of data
                            int tOutputBufferSize = tChunkBufferSize;
                            int tBytesDecoded = 0;

                            if (mAudioResampleContext != NULL)
                            {// audio resampling needed
                                // decode the input audio buffer to the resample buffer
                                tBytesDecoded = HM_avcodec_decode_audio(mCodecContext, (int16_t *)mResampleBuffer, &tOutputBufferSize, tPacket);
                            }else
                            {// no audio resampling
                                // decode the input audio buffer straight to the output buffer
                                tBytesDecoded = HM_avcodec_decode_audio(mCodecContext, (int16_t *)tChunkBuffer, &tOutputBufferSize, tPacket);
                            }


                            #ifdef MSMEM_DEBUG_AUDIO_FRAME_RECEIVER
                                LOG(LOG_VERBOSE, "New audio frame..");
                                LOG(LOG_VERBOSE, "      ..samples: %d", tOutputBufferSize);
                                LOG(LOG_VERBOSE, "      ..size: %d bytes", tOutputBufferSize * tOutoutAudioBytesPrSample);
                            #endif

                            if (mAudioResampleContext != NULL)
                            {// audio resampling needed: we have to insert an intermediate step, which resamples the audio chunk

                                if(tOutputBufferSize > 0)
                                {
                                    // ############################
                                    // ### RESAMPLE FRAME (CONVERT)
                                    // ############################
                                    //HINT: we always assume 16 bit samples
                                    int tResampledBytes = (tOutoutAudioBytesPrSample * mOutputAudioChannels) * audio_resample(mAudioResampleContext, (short*)tChunkBuffer, (short*)mResampleBuffer, tOutputBufferSize / (tInputAudioBytesPrSample * mInputAudioChannels));
                                    #ifdef MSMEM_DEBUG_AUDIO_FRAME_RECEIVER
                                        LOG(LOG_VERBOSE, "Have resampled %d bytes of sample rate %dHz and %d channels to %d bytes of %d Hz sample rate and %d channels", tOutputBufferSize, mCodecContext->sample_rate, mCodecContext->channels, tResampledBytes, mOutputAudioSampleRate, mOutputAudioChannels);
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
                            }else
                            {// no audio resampling needed
                                // we can use the input buffer without modifications
                                tCurrentChunkSize = tOutputBufferSize;
                            }

                            if (tCurrentChunkSize > 0)
                            {
                                // ############################
                                // ### WRITE FRAME TO FIFO
                                // ############################
                                // increase fifo buffer size by size of input buffer size
                                #ifdef MSMEM_DEBUG_PACKETS
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
                                #ifdef MSMEM_DEBUG_PRE_BUFFERING
                                    LOG(LOG_VERBOSE, "Setting current frame PTS to packet PTS: %ld", tCurPacketPts);
                                #endif
                                tCurFramePts = tCurPacketPts;
//-
                                int tLoops = 0;
                                while (av_fifo_size(tSampleFifo) >= MEDIA_SOURCE_SAMPLES_BUFFER_SIZE)
                                {
                                    tLoops++;
                                    // ############################
                                    // ### READ FRAME FROM FIFO (max. 1024 samples)
                                    // ############################
                                    #ifdef MSM_DEBUG_PACKETS
                                        LOG(LOG_VERBOSE, "Loop %d-Reading %d bytes from %d bytes of fifo, current PTS: %ld", tLoops, MSF_DESIRED_AUDIO_INPUT_SIZE, av_fifo_size(tSampleFifo), tCurFramePts);
                                    #endif
                                    // read sample data from the fifo buffer
                                    HM_av_fifo_generic_read(tSampleFifo, (void*)tChunkBuffer, MEDIA_SOURCE_SAMPLES_BUFFER_SIZE);
                                    tCurrentChunkSize = MEDIA_SOURCE_SAMPLES_BUFFER_SIZE;

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
                                        #ifdef MSMEM_DEBUG_AUDIO_FRAME_RECEIVER
                                            LOG(LOG_VERBOSE, "Writing %d %s bytes at %p to output FIFO with PTS %ld, remaining audio data: %d bytes", tCurrentChunkSize, GetMediaTypeStr().c_str(), tChunkBuffer, tCurFramePts, av_fifo_size(tSampleFifo));
                                        #endif
                                        WriteFrameOutputBuffer((char*)tChunkBuffer, tCurrentChunkSize, tCurFramePts);
                                        #ifdef MSMEM_DEBUG_DECODER_STATE
                                            LOG(LOG_VERBOSE, "Successful audio buffer loop");
                                        #endif
                                    }else
                                    {
                                        LOG(LOG_ERROR, "Cannot write a %s chunk of %d bytes to the FIFO with %d bytes slots", GetMediaTypeStr().c_str(),  tCurrentChunkSize, mDecoderFifo->GetEntrySize());
                                    }
                                }

                                // reset chunk size to avoid additional writes to output FIFO because we already stored all valid audio buffers in output FIFO
                                tCurrentChunkSize = 0;
                                #ifdef MSMEM_DEBUG_PACKETS
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
                    #ifdef MSMEM_DEBUG_PACKETS
                        LOG(LOG_VERBOSE, "Writing %d %s bytes at %p to FIFO with PTS %ld", tCurrentChunkSize, GetMediaTypeStr().c_str(), tChunkBuffer, tCurFramePts);
                    #endif
                    if (tCurrentChunkSize <= mDecoderFifo->GetEntrySize())
                    {
                        WriteFrameOutputBuffer((char*)tChunkBuffer, tCurrentChunkSize, tCurFramePts);
                        #ifdef MSMEM_DEBUG_PACKETS
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

            mDecoderNeedWorkConditionMutex.lock();
            if (mEOFReached)
            {// EOF, wait until restart
                // add empty chunk to FIFO
                #ifdef MSMEM_DEBUG_PACKETS
                    LOG(LOG_VERBOSE, "EOF reached, writing empty chunk to %s stream decoder FIFO", GetMediaTypeStr().c_str());
                #endif

                char tData[4];
                WriteFrameOutputBuffer(tData, 0, 0);

                // time to sleep
                #ifdef MSMEM_DEBUG_DECODER_STATE
                    LOG(LOG_VERBOSE, "EOF for %s source reached, wait some time and check again, loop %d", GetMediaTypeStr().c_str(), ++tWaitLoop);
                #endif
                mDecoderNeedWorkCondition.Reset();
                mDecoderNeedWorkCondition.Wait(&mDecoderNeedWorkConditionMutex);
                mDecoderLastReadPts = 0;
                mEOFReached = false;
                #ifdef MSMEM_DEBUG_DECODER_STATE
                    LOG(LOG_VERBOSE, "Continuing after stream decoding was restarted");
                #endif
            }
            mDecoderNeedWorkConditionMutex.unlock();
        }else
        {// decoder FIFO is full, nothing to be done
            #ifdef MSMEM_DEBUG_DECODER_STATE
                if(mDecoderFifo != NULL)
                    LOG(LOG_VERBOSE, "Nothing to do for %s decoder, FIFO has %d of %d entries, wait some time and check again, loop %d", GetMediaTypeStr().c_str(), mDecoderFifo->GetUsage(), mDecoderFifo->GetSize(), ++tWaitLoop);
                else
                    LOG(LOG_VERBOSE, "Nothing to do for %s decoder, wait some time and check again, loop %d", GetMediaTypeStr().c_str(), ++tWaitLoop);
            #endif
            mDecoderNeedWorkCondition.Reset();
            mDecoderNeedWorkCondition.Wait(&mDecoderNeedWorkConditionMutex);
            #ifdef MSMEM_DEBUG_DECODER_STATE
                if(mDecoderFifo != NULL)
                    LOG(LOG_VERBOSE, "Continuing after new data is needed, current FIFO size is: %d of %d", mDecoderFifo->GetUsage(), mDecoderFifo->GetSize());
                else
                    LOG(LOG_VERBOSE, "Continuing after new data is needed");
            #endif
            mDecoderNeedWorkConditionMutex.unlock();
        }
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

    LOG(LOG_WARN, "%s decoder main loop finished for %s media source <<<<<<<<<<<<<<<<", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());

    return NULL;
}

void MediaSourceMem::WriteFrameOutputBuffer(char* pBuffer, int pBufferSize, int64_t pPts)
{
    if (mDecoderFifo == NULL)
        LOG(LOG_ERROR, "Invalid decoder FIFO");

    #ifdef MSMEM_DEBUG_PACKETS
        LOG(LOG_VERBOSE, ">>> Writing frame of %d bytes and pts %ld", pBufferSize, pPts);
    #endif

    // write A/V data to output FIFO
    mDecoderFifo->WriteFifo(pBuffer, pBufferSize);

    // add meta description about current chunk to different FIFO
    struct ChunkDescriptor tChunkDesc;
    tChunkDesc.Pts = pPts;
    mDecoderMetaDataFifo->WriteFifo((char*) &tChunkDesc, sizeof(tChunkDesc));

    // update pre-buffer time value
    UpdateBufferTime();
}

void MediaSourceMem::ReadFrameOutputBuffer(char *pBuffer, int &pBufferSize, int64_t &pPts)
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

    //LOG(LOG_VERBOSE, "Returning from decoder FIFO the %s frame (PTS = %ld)", GetMediaTypeStr().c_str(), pPts);

    // update pre-buffer time value
    UpdateBufferTime();
}

bool MediaSourceMem::DecoderFifoFull()
{
	return ((mDecoderFifo == NULL) || (mDecoderFifo->GetUsage() >= mDecoderFifo->GetSize() - 1 /* one slot for a 0 byte signaling chunk*/) /* meta data FIFO has always the same size => hence, we don't have to check its size */ ||
			(mDecoderMetaDataFifo == NULL) || (mDecoderMetaDataFifo->GetUsage() >= mDecoderMetaDataFifo->GetSize() - 1));
}

void MediaSourceMem::UpdateBufferTime()
{
    //LOG(LOG_VERBOSE, "Updating pre-buffer time value");

    float tBufferSize = 0;
    if (mDecoderFifo != NULL)
        tBufferSize = mDecoderFifo->GetUsage();

    mDecoderFrameBufferTime = tBufferSize / GetFrameRatePlayout();
}

void MediaSourceMem::CalibrateRTGrabbing()
{
    // adopt the stored pts value which represent the start of the media presentation in real-time useconds
    float  tRelativeFrameIndex = mGrabberCurrentFrameIndex - mSourceStartPts;
    double tRelativeTime = (int64_t)((double)AV_TIME_BASE * tRelativeFrameIndex / GetFrameRate());
    #ifdef MSMEM_DEBUG_CALIBRATION
        LOG(LOG_WARN, "Calibrating %s RT playback, old PTS start: %.2f", GetMediaTypeStr().c_str(), mSourceStartTimeForRTGrabbing);
    #endif
    mSourceStartTimeForRTGrabbing = av_gettime() - tRelativeTime  + mDecoderFramePreBufferTime * AV_TIME_BASE;
    #ifdef MSMEM_DEBUG_CALIBRATION
        LOG(LOG_WARN, "Calibrating %s RT playback: new PTS start: %.2f, rel. frame index: %.2f, rel. time: %.2f ms", GetMediaTypeStr().c_str(), mSourceStartTimeForRTGrabbing, tRelativeFrameIndex, (float)(tRelativeTime / 1000));
    #endif
}

void MediaSourceMem::WaitForRTGrabbing()
{
    // calculate the current (normalized) frame index of the grabber
    float tNormalizedFrameIndexFromGrabber = mGrabberCurrentFrameIndex - mSourceStartPts; // the normalized frame index

    // the PTS value of the last output frame
    uint64_t tCurrentPtsFromGrabber = (uint64_t)(1000 * tNormalizedFrameIndexFromGrabber / GetFrameRate()); // in ms

    // calculate the PTS offset between the RTCP PTS reference and the last grabbed frame
    int64_t tDesiredPlayOutTime = 1000 * ((int64_t)tCurrentPtsFromGrabber); // in us

    // calculate the current (normalized) play-out time of the current A/V stream
    int64_t tCurrentPlayOutTime = av_gettime() - (int64_t)mSourceStartTimeForRTGrabbing; // in us

    // calculate the time offset between the desired and current play-out time, which can be used for a wait cycle (Thread::Suspend)
    int64_t tResultingTimeOffset = tDesiredPlayOutTime - tCurrentPlayOutTime; // in us

    #ifdef MSMEM_DEBUG_TIMING
        LOG(LOG_VERBOSE, "%s-current relative frame index: %f, relative time: %f us (Fps: %3.2f), stream start time: %f us, packet's relative play out time: %f us, time difference: %f us", GetMediaTypeStr().c_str(), tNormalizedFrameIndexFromGrabber, tCurrentRelativeTimeIndexInUSecs, GetFrameRate(), (float)mSourceStartPts, tDesiredPlayOutTime, tResultingTimeOffset);
    #endif
    // adapt timing to real-time
    if (tResultingTimeOffset > 0)
    {
        #ifdef MSMEM_DEBUG_TIMING
            LOG(LOG_WARN, "%s-sleeping for %ld ms for frame %.2f", GetMediaTypeStr().c_str(), tResultingTimeOffset / 1000, (float)mGrabberCurrentFrameIndex);
        #endif
		if (tResultingTimeOffset <= MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME * AV_TIME_BASE)
			Thread::Suspend(tResultingTimeOffset);
		else
		{
			LOG(LOG_WARN, "Found in %s %s source an invalid delay time of %ld s, pre-buffer time: %.2f", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), tResultingTimeOffset / 1000, mDecoderFramePreBufferTime);
			LOG(LOG_WARN, "Re-calibrating RT grabbing");
			CalibrateRTGrabbing();
		}
	}else
    {
        #ifdef MSMEM_DEBUG_TIMING
            if (tResultingTimeOffset < -MEDIA_SOURCE_MEM_VIDEO_FRAME_DROP_THRESHOLD)
                LOG(LOG_VERBOSE, "System too slow?, %s-grabbing is %ld ms too late", GetMediaTypeStr().c_str(), tResultingTimeOffset / (-1000));
        #endif
    }
}

int64_t MediaSourceMem::GetSynchronizationTimestamp()
{
    int64_t tResult = 0;

    if ((mGrabberCurrentFrameIndex != 0) && (!mDecoderRecalibrateRTGrabbingAfterSeeking))
    {// we have some first passed A/V frames, the decoder does not need to re-calibrate the RT grabber
        /******************************************
         * The following lines do the following:
         *   - use the RTCP sender reports and extract the reference RTP timestamps and NTP time
         *     =>> we know when exactly (NTP time!) a given RTP play-out time index passed the processing chain at sender side
         *   - interpolate the given NTP time from sender side to conclude the NTP time (at sender side!) for the currently grabbed frame
         *     =>> we know (this is the hope!) exactly when the current frame passed the processing chain at sender side
         *   - return the calculated NTP time as synchronization timestamp in micro seconds
         *
         *   - when this approach is applied for both the video and audio stream (which is received from the same physical host with the same real-time clock inside!),
         *     a time difference can be derived which corresponds to the A/V drift in micro seconds of the video and audio playback on the local(!) machine
         ******************************************/
        if (mRtpActivated)
        {// RTP active
            // get the reference from RTCP
            uint64_t tReferenceNtpTime;
            unsigned int tReferencePts;
            GetSynchronizationReferenceFromRTP(tReferenceNtpTime, tReferencePts);

            // calculate the current (normalized) frame index of the grabber
            float tNormalizedFrameIndexFromGrabber = mGrabberCurrentFrameIndex - mSourceStartPts; // the normalized frame index

            // the PTS value of the last output frame
            uint64_t tCurrentPtsFromGrabber = (uint64_t)(1000 * tNormalizedFrameIndexFromGrabber / GetFrameRatePlayout()); // in ms

            // calculate the PTS offset between the RTCP PTS reference and the last grabbed frame
            int64_t tTimeOffsetFromReference = 1000 * ((int64_t)tCurrentPtsFromGrabber - tReferencePts); // in us

            if (tReferenceNtpTime != 0)
            {
                // calculate the NTP time (we refer to the NTP time of the RTP source) of the currently grabbed frame
                tResult = (int64_t)tReferenceNtpTime + tTimeOffsetFromReference;
                int64_t tLocalNtpTime = av_gettime();

                #ifdef MSMEM_DEBUG_AV_SYNC
					// the PTS value of the last (received) input frame
					uint64_t tCurrentPtsFromRTP = GetCurrentPtsFromRTP(); // in ms

                	// calculate the play time of the RTP source [in us]
					uint64_t tCurrentPlayTimeofRTPSource = ((uint64_t) tReferencePts /* playout time in ms */) * 1000; // in us

                	LOG(LOG_VERBOSE, "%s reference PTS from RTP: %u, PTS from grabber: %lu(frame: %lu, play-out fps: %.2f), PTS from RTP: %lu(frame: %lu), play time from RTP source: %.2f s", GetMediaTypeStr().c_str(), tReferencePts, tCurrentPtsFromGrabber, (uint64_t)mGrabberCurrentFrameIndex, GetFrameRatePlayout(), tCurrentPtsFromRTP, CalculateFrameNumberFromRTP(), (float)tCurrentPlayTimeofRTPSource / AV_TIME_BASE);
                    LOG(LOG_VERBOSE, "%s reference NTP time: %10lu, offset: %10ld (reference PTS: %10u, grabber PTS: %10lu), resulting synch. timestamp: %10ld", GetMediaTypeStr().c_str(), tReferenceNtpTime, tTimeOffsetFromReference * 1000, tReferencePts, tCurrentPtsFromGrabber, tResult);
                    //HINT: "diff" value should correlate with the frame buffer time! otherwise something went wrong in the processing chain
                    LOG(LOG_VERBOSE, "         local timestamp: %10ld                                                                                      local timestamp: %10ld, diff: %ld ms", tLocalNtpTime, tLocalNtpTime, (tLocalNtpTime - tResult) / 1000);
                #endif
                //if (tCurrentPtsFromRTP < tReferencePts)
                //    LOG(LOG_WARN, "Received %s reference (from RTP) PTS value: %u is in the future, last received (RTP) PTS value: %lu)", GetMediaTypeStr().c_str(), tReferencePts, tCurrentPtsFromRTP);
            }else
            {// reference values from RTP are still invalid, RTCP packet is needed (expected in some seconds)
                // nothing to complain about, we return 0 to signal we have no valid synchronization timestamp yet
            }
        }else
        {// no RTP available, we have plain network transport of a file based A/V media source
            // ..
        }
    }

    return tResult;
}

bool MediaSourceMem::TimeShift(int64_t pOffset)
{
    LOG(LOG_VERBOSE, "Shifting %s time by: %ld", GetMediaTypeStr().c_str(), pOffset);
    mSourceStartTimeForRTGrabbing -= pOffset;
    return true;
}

uint64_t MediaSourceMem::CalculateFrameNumberFromRTP()
{
    uint64_t tResult = 0;
    float tFrameRate = GetFrameRatePlayout();
    if (tFrameRate < 1.0)
        return 0;

    // the following sequence delivers the frame number independent from packet loss because
    // it calculates the frame number based on the current timestamp from the RTP header
    float tTimeBetweenFrames = 1000 / tFrameRate;
    tResult = rint((float) GetCurrentPtsFromRTP()/ tTimeBetweenFrames);

    #ifdef MSMEM_DEBUG_PRE_BUFFERING
        LOG(LOG_VERBOSE, "Calculated a frame number: %ld (RTP timestamp: %ld), fps: %.2f", tResult, GetCurrentPtsFromRTP(), tFrameRate);
    #endif

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
