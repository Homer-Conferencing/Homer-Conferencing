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

// timeout until we give up to find a suitable A/V frame in GrabChunk()
#define MEDIA_SOURCE_MEM_GRABBING_TIMEOUT                                   0.25 // seconds

// assumed default jitter for end-to-end delay for an A/V transmission
#define MEDIA_SOURCE_MEM_DEFAULT_E2E_DELAY_JITER                            0.1 //seconds

// how much time do we want to buffer at maximum?
#define MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME                         ((System::GetTargetMachineType() != "x86") ? 6.0 : 2.0) // use less memory for 32 bit targets

// how long do we want to wait for the first key frame when starting decoder loop?
#define MSM_WAITING_FOR_FIRST_KEY_FRAME_TIMEOUT                             7 // seconds

// how much delay for frame playback do we allow before we drop the frame?
#define MEDIA_SOURCE_MEM_FRAME_DROP_THRESHOLD                               0.4 // seconds

///////////////////////////////////////////////////////////////////////////////

MediaSourceMem::MediaSourceMem(string pName):
    MediaSource(pName), RTP()
{
    mDecoderFrameBufferTimeMax = MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME;
    mDecoderFramePreBufferTime = MEDIA_SOURCE_MEM_DEFAULT_E2E_DELAY_JITER;
    mDecoderTargetOutputFrameIndex = 0;
    mDecoderRecalibrateRTGrabbingAfterSeeking = true;
    mDecoderSinglePictureGrabbed = false;
    mFirstReceivedFrameTimestampFromRTP = -1;
    mDecoderThreadAcountsPackets = true;
    mDecoderFifo = NULL;
    mRtpActivated = false;
    mDecoderFragmentFifo = NULL;
    mResXLastGrabbedFrame = 0;
    mResYLastGrabbedFrame = 0;
    mDecoderSinglePictureResX = 0;
    mDecoderSinglePictureResY = 0;
    mDecoderUsesPTSFromInputPackets = false;
    mCurrentOutputFrameIndex = -1;
    mLastBufferedOutputFrameIndex = 0;
    mLastTimeWaitForRTGrabbing = 0;
    mTimeLastWrittenOutputChunk = 0;
    mWrappingHeaderSize= 0;
    mGrabberProvidesRTGrabbing = true;
    mSourceType = SOURCE_MEMORY;
    mStreamPacketBuffer = (char*)malloc(MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE);
    mFragmentBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);
    mFragmentNumber = 0;
    mPacketStatAdditionalFragmentSize = 0;
    mOpenInputStream = false;
    RTPRegisterPacketStatistic(this);

    mRtpSourceCodecIdHint = AV_CODEC_ID_NONE;
    mSourceCodecId = AV_CODEC_ID_NONE;

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
    if (mDecoderFifo != NULL)
    {
        delete mDecoderFifo;
        mDecoderFifo = NULL;
    }
    mDecoderFragmentFifoDestructionMutex.unlock();
    free(mStreamPacketBuffer);
    free(mFragmentBuffer);
}

///////////////////////////////////////////////////////////////////////////////
// static call back function:
//      function reads frame packet and delivers it upwards to ffmpeg library
// HINT: result can consist of many network packets, which represent one frame (packet)
int MediaSourceMem::GetNextInputFrame(void *pOpaque, uint8_t *pBuffer, int pBufferSize)
{
    MediaSourceMem *tMediaSourceMemInstance = (MediaSourceMem*)pOpaque;
    char *tBuffer = (char*)pBuffer;
    int tBufferSize = pBufferSize;
    int64_t tFragmentNumber;

    #ifdef MSMEM_DEBUG_PACKETS
        LOGEX(MediaSourceMem, LOG_VERBOSE, "Got a call for GetNextPacket() with a packet buffer at %p and size of %d bytes", pBuffer, pBufferSize);
    #endif
    if (tMediaSourceMemInstance->mRtpActivated)
    {// rtp is active, fragmentation possible!
     // we have to parse every incoming network packet and create a frame packet from the network packets (fragments)
        char *tFragmentData;
        int tFragmentBufferSize;
        int tFragmentDataSize;
        bool tLastFragmentOfAVPacket;
        bool tFragmenHasAVData;
        enum RtcpType tFragmentRtcpType;

        tBufferSize = 0;

        do{
            tLastFragmentOfAVPacket = false;
            tFragmenHasAVData = false;
            tFragmentData = &tMediaSourceMemInstance->mFragmentBuffer[0];
            tFragmentBufferSize = MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE; // maximum size of one single fragment of a frame packet
            // receive a fragment
            tMediaSourceMemInstance->ReadFragment(tFragmentData, tFragmentBufferSize, tFragmentNumber);

            // create new AV packet
            AVPacket tAVPacket;
            av_init_packet(&tAVPacket);
            tAVPacket.data = (uint8_t*)tFragmentData;
            tAVPacket.size = tFragmentBufferSize;
            /* assume every frame as key frame, we simply relay hop-by-hop */
            tAVPacket.flags |= AV_PKT_FLAG_KEY;

            // relay the received fragment to registered sinks
            tMediaSourceMemInstance->RelayAVPacketToMediaSinks(&tAVPacket);

            #ifdef MSMEM_DEBUG_PACKET_RECEIVER
                LOGEX(MediaSourceMem, LOG_VERBOSE, "Got packet fragment of size %d at address %p", tFragmentBufferSize, tFragmentData);
            #endif
//            for (unsigned int i = 0; i < 64; i++)
//                if (tFragmentBufferSize >= i)
//                    LOG(LOG_VERBOSE, "stream data (%2u): RTP+12 %02hx(%3d)  RTP %02hx(%3d)", i, tFragmentData[i + 12] & 0xFF, tFragmentData[i + 12] & 0xFF, tFragmentData[i] & 0xFF, tFragmentData[i] & 0xFF);
            if (tMediaSourceMemInstance->mGrabbingStopped)
            {
                LOGEX(MediaSourceMem, LOG_WARN, "%s-Grabbing was stopped", tMediaSourceMemInstance->GetMediaTypeStr().c_str());
                return AVERROR(ENODEV); //force negative resulting buffer size to signal error and force a return to the calling GUI!
            }
            if (tFragmentBufferSize < 0)
            {
                LOGEX(MediaSourceMem, LOG_ERROR, "Received invalid fragment");
                return 0;
            }
            if (tFragmentBufferSize > 0)
            {
                tFragmentDataSize = tFragmentBufferSize;
                // parse and remove the RTP header, extract the encapsulated frame fragment
                tFragmenHasAVData = tMediaSourceMemInstance->RtpParse(tFragmentData, tFragmentDataSize, tLastFragmentOfAVPacket, tFragmentRtcpType, tMediaSourceMemInstance->mSourceCodecId, false);
                #ifdef MSMEM_DEBUG_PACKETS
                    LOGEX(MediaSourceMem, LOG_VERBOSE, "Got %d bytes %s payload from %d bytes RTP packet", tFragmentDataSize, GetGuiNameFromCodecID(tMediaSourceMemInstance->mSourceCodecId).c_str(), tFragmentBufferSize);
                #endif
                // relay new data to registered sinks
                if(tFragmenHasAVData)
                {// fragment is okay
                    // store the received fragment locally
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
                        tLastFragmentOfAVPacket = false;
                        tBuffer = (char*)pBuffer;
                        tBufferSize = pBufferSize;
                        LOGEX(MediaSourceMem, LOG_ERROR, "Stream buffer of %d bytes too small for input, dropping received stream", pBufferSize);
                    }
                }else
                {// fragment has no valid A/V data
                    if (tFragmentRtcpType > 0)
                    {// we have received a sender report or a sender description
                        // nothing to do here
                    }else
                    {// we have received an unsupported RTCP packet/RTP payload or something went completely wrong
                        if (tMediaSourceMemInstance->HasInputStreamChanged())
                        {// we have to reset the source
                            LOGEX(MediaSourceMem, LOG_WARN, "Detected source change at remote side, signaling EOF and returning immediately");
                            return AVERROR(ENODEV);
                        }else
                        {// something went wrong
                            LOGEX(MediaSourceMem, LOG_WARN, "Current RTP packet was reported as invalid by RTP parser, ignoring this data");
                        }
                    }
                }
            }else
            {
                if (tMediaSourceMemInstance->mGrabbingStopped)
                {
                    LOGEX(MediaSourceMem, LOG_WARN, "Detected empty signaling fragment, grabber should be stopped, returning zero data");
                    return 0;
                }else if (!tMediaSourceMemInstance->mDecoderThreadNeeded)
                {
                    LOGEX(MediaSourceMem, LOG_WARN, "Detected empty signaling fragment, decoder should be stopped, returning zero data");
                    return 0;
                }else
                    LOGEX(MediaSourceMem, LOG_WARN, "Detected empty signaling fragment, ignoring it");
            }
        }while(!tLastFragmentOfAVPacket);
    }else
    {// rtp is inactive
        tMediaSourceMemInstance->ReadFragment(tBuffer, tBufferSize, tFragmentNumber);
        if (tMediaSourceMemInstance->mGrabbingStopped)
        {
            LOGEX(MediaSourceMem, LOG_WARN, "%s-Grabbing was stopped", tMediaSourceMemInstance->GetMediaTypeStr().c_str());
            return AVERROR(ENODEV); //force negative resulting buffer size to signal error and force a return to the calling GUI!
        }

        if (tBufferSize < 0)
        {
            LOGEX(MediaSourceMem, LOG_ERROR, "Error when receiving network data");
            return 0;
        }

        // create new AV packet
        AVPacket tAVPacket;
        av_init_packet(&tAVPacket);
        tAVPacket.data = (uint8_t*)tBuffer;
        tAVPacket.size = tBufferSize;

        // relay the received fragment to registered sinks
        tMediaSourceMemInstance->RelayAVPacketToMediaSinks(&tAVPacket);
    }

    #ifdef MSMEM_DEBUG_AUDIO_FRAME_RECEIVER
        if (tMediaSourceMemInstance->GetMediaType() == MEDIA_AUDIO)
            LOGEX(MediaSourceMem, LOG_WARN, "Returning %s frame of size %d", tMediaSourceMemInstance->GetMediaTypeStr().c_str(), tBufferSize);
    #endif
    #ifdef MSMEM_DEBUG_VIDEO_FRAME_RECEIVER
        if (tMediaSourceMemInstance->GetMediaType() == MEDIA_VIDEO)
            LOGEX(MediaSourceMem, LOG_WARN, "Returning %s frame of size %d", tMediaSourceMemInstance->GetMediaTypeStr().c_str(), tBufferSize);
    #endif

    // ###############################################################
    // ### if multiple frames are delivered to the decoder thread in one big piece, we need the time reference of the first frame for RT-grabbing and synchronous playback
    // ###############################################################
    if (tMediaSourceMemInstance->mFirstReceivedFrameTimestampFromRTP == -1)
        tMediaSourceMemInstance->mFirstReceivedFrameTimestampFromRTP = (double)tMediaSourceMemInstance->GetCurrentPtsFromRTP();

    return tBufferSize;
}

int64_t MediaSourceMem::GetEndToEndDelay()
{
    return mRtcpEndToEndDelay;
}

float MediaSourceMem::GetRelativeLoss()
{
    return mRtcpRelativeLoss;
}

void MediaSourceMem::WriteFragment(char *pBuffer, int pBufferSize, int64_t pFragmentNumber)
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

        #ifdef MSMEM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Write fragment at %p with size %5d for %s decoder", pBuffer, pBufferSize, GetMediaTypeStr().c_str());
            if (pBufferSize > 7)
                LOG(LOG_VERBOSE, "First eight bytes are: %hhx %hhx %hhx %hhx  %hhx %hhx %hhx %hhx", pBuffer[0], pBuffer[1], pBuffer[2], pBuffer[3], pBuffer[4], pBuffer[5], pBuffer[6], pBuffer[7]);
        #endif
    }

    if (mDecoderFragmentFifo->GetUsage() >= mDecoderFragmentFifo->GetSize() - 4)
    {
        LOG(LOG_WARN, "Decoder fragment FIFO is near overload situation in WriteFragmet(), deleting all stored fragments");

        // delete all stored frames: it is a better for the decoding!
        mDecoderFragmentFifo->ClearFifo();
    }

    mDecoderFragmentFifo->WriteFifo(pBuffer, pBufferSize, pFragmentNumber);
}

void MediaSourceMem::ReadFragment(char *pBuffer, int &pBufferSize, int64_t &pFragmentNumber)
{
    if (mDecoderFragmentFifo == NULL)
    {
        return;
    }

    mDecoderFragmentFifo->ReadFifo(&pBuffer[0], pBufferSize, pFragmentNumber);

    if (pBufferSize > 0)
    {
        #ifdef MSMEM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Read fragment with number %5u at %p with size %5d towards %s decoder", (unsigned int)++mFragmentNumber, pBuffer, pBufferSize, GetMediaTypeStr().c_str());
        #endif
    }

    // is FIFO near overload situation?
    if (mDecoderFragmentFifo->GetUsage() >= mDecoderFragmentFifo->GetSize() - 4)
    {
        LOG(LOG_WARN, "Decoder fragment FIFO is near overload situation in ReadFragment(), deleting all stored fragments");

        // delete all stored frames: it is a better for the decoding!
        mDecoderFragmentFifo->ClearFifo();
    }
}

std::string MediaSourceMem::GetBroadcasterName()
{
    string tResult = "";

    /*
     * use data from the icecast/shoutcast protocol
     */
    MetaData tMetaData = GetMetaData();
    for(unsigned int i = 0; i < tMetaData.size(); i++)
    {
        if (tMetaData[i].Key == "icy-name")
        {
           tResult = tMetaData[i].Value;
           break;
        }
    }

    return tResult;
}

std::string MediaSourceMem::GetBroadcasterStreamName()
{
    string tResult = "";

    /*
     * use data from RTCP sender descriptions
     */
    if(mRtpActivated)
        tResult = mRtcpSenderDescription;

    /*
     * use data from the icecast/shoutcast protocol
     */
    MetaData tMetaData = GetMetaData();
    for(unsigned int i = 0; i < tMetaData.size(); i++)
    {
        if (tMetaData[i].Key == "StreamTitle")
        {
           tResult = tMetaData[i].Value;
           break;
        }
    }

    return tResult;
}

std::string MediaSourceMem::GetCurrentDeviceName()
{
    return MediaSource::GetCurrentDeviceName() + (mRtcpSenderDescription != "" ? + " (" + mRtcpSenderDescription + ")" : "");
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

        tFormat.Name="VGA";       //
        tFormat.ResX = 640;
        tFormat.ResY = 480;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="CIF4";       //      704 � 576
        tFormat.ResX = 704;
        tFormat.ResY = 576;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="DVD";        //      720 � 576
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

        tFormat.Name="CIF9";       //     1056 � 864
        tFormat.ResX = 1056;
        tFormat.ResY = 864;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="SXGA";       //     1280 � 1024
        tFormat.ResX = 1280;
        tFormat.ResY = 1024;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="WXGA+";      //     1440 � 900
        tFormat.ResX = 1440;
        tFormat.ResY = 900;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="SXGA+";       //     1440 � 1050
        tFormat.ResX = 1440;
        tFormat.ResY = 1050;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="CIF16";      //     1408 � 1152
        tFormat.ResX = 1408;
        tFormat.ResY = 1152;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="HDTV";       //     1920 � 1080
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
    LOG(LOG_VERBOSE, "Ignoring %f, keeping FPS of %f", pFps, GetInputFrameRate());
}

bool MediaSourceMem::SupportsRecording()
{
    return true;
}

void MediaSourceMem::RegisterMediaFilter(MediaFilter *pMediaFilter)
{
    MediaSource::RegisterMediaFilter(pMediaFilter);
    ResetPreCalculatedData();
}

bool MediaSourceMem::UnregisterMediaFilter(MediaFilter *pMediaFilter, bool pAutoDelete)
{
    bool tResult = MediaSource::UnregisterMediaFilter(pMediaFilter, pAutoDelete);

    ResetPreCalculatedData();

    return tResult;
}

bool MediaSourceMem::SupportsRelaying()
{
    return true;
}

bool MediaSourceMem::HasInputStreamChanged()
{
    bool tResult = HasSourceChangedFromRTP();
    enum AVCodecID tNewCodecId = AV_CODEC_ID_NONE;

    // try to detect the right source codec based on RTP data
    if ((tResult) && (mRtpActivated))
    {
        switch(GetRTPPayloadType())
        {
            //video
            case 31:
                    tNewCodecId = AV_CODEC_ID_H261;
                    break;
            case 32:
                    tNewCodecId = AV_CODEC_ID_MPEG2VIDEO;
                    break;
            case 34:
            case 118:
                    tNewCodecId = AV_CODEC_ID_H263;
                    break;
            case 119:
                    tNewCodecId = AV_CODEC_ID_H263P;
                    break;
            case 120:
                    tNewCodecId = AV_CODEC_ID_H264;
                    break;
            case 121:
                    tNewCodecId = AV_CODEC_ID_MPEG4;
                    break;
            case 122:
                    tNewCodecId = AV_CODEC_ID_THEORA;
                    break;
            case 123:
                    tNewCodecId = AV_CODEC_ID_VP8;
                    break;
            case 124:
                    tNewCodecId = AV_CODEC_ID_HEVC;
                    break;

            //audio
            case 0:
                    tNewCodecId = AV_CODEC_ID_PCM_MULAW;
                    break;
            case 3:
                    tNewCodecId = AV_CODEC_ID_GSM;
                    break;
            case 8:
                    tNewCodecId = AV_CODEC_ID_PCM_ALAW;
                    break;
            case 9:
                    tNewCodecId = AV_CODEC_ID_ADPCM_G722;
                    break;
            case 10:
                    tNewCodecId = AV_CODEC_ID_PCM_S16BE;
                    break;
            case 11:
                    tNewCodecId = AV_CODEC_ID_PCM_S16BE;
                    break;
            case 14:
                    tNewCodecId = AV_CODEC_ID_MP3;
                    break;
            case 100:
                    tNewCodecId = AV_CODEC_ID_AAC;
                    break;
            case 101:
                    tNewCodecId = AV_CODEC_ID_AMR_NB;
                    break;
            default:
                    break;
        }
        if ((tNewCodecId != AV_CODEC_ID_NONE) && (tNewCodecId != mSourceCodecId))
        {
            LOG(LOG_VERBOSE, "Suggesting codec change from %d(%s) to %d(%s)", mSourceCodecId, HM_avcodec_get_name(mSourceCodecId), tNewCodecId, HM_avcodec_get_name(tNewCodecId));
            mRtpSourceCodecIdHint = tNewCodecId;
        }
    }

    return tResult;
}

void MediaSourceMem::StopGrabbing()
{
    LOG(LOG_VERBOSE, "Stopping grabber");

    MediaSource::StopGrabbing();

    if ((!mMediaSourceOpened) && (!mOpenInputStream))
    {
        LOG(LOG_VERBOSE, "Stopping grabber aborted - source wasn't started yet");
        return;
    }

    WriteFragment(NULL, 0, 0);

    int tLoop = 0;
    int64_t tTime = Time::GetTimeStamp();
    do{
        if (tLoop > 0)
            LOG(LOG_VERBOSE, "Attempt %d to stop %s %s grabbing", tLoop, GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());
        tLoop++;

        WriteOutputChunk(NULL, 0, 0);
    }while(!mGrabMutex.lock(250));

    // now, the grabber returned and the lock was correctly acquired -> unlock again
    mGrabMutex.unlock();

    LOG(LOG_VERBOSE, "Got %s %s decoder stopped after %lld ms", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), (Time::GetTimeStamp() - tTime) / 1000);

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
    //HINT: reserve one buffer for internal signaling
    switch(GetMediaType())
    {
        case MEDIA_AUDIO:
            tResult = rint(mDecoderFrameBufferTimeMax * mOutputFrameRate /* 44100 samples per second / 1024 samples per frame */);
            break;
        case MEDIA_VIDEO:
            tResult = rint(mDecoderFrameBufferTimeMax * mOutputFrameRate /* r_frame_rate.num / r_frame_rate.den from the AVStream */);
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

        if (IsRunning())
        {
            LOG(LOG_VERBOSE, "Stopping decoder from DoSetVideoGrabResolution()");
            StopDecoder();
            LOG(LOG_VERBOSE, "Starting decoder from DoSetVideoGrabResolution()");
            StartDecoder();
        }
    }
    LOG(LOG_VERBOSE, "DoSetVideoGrabResolution() finished");
}

bool MediaSourceMem::SetInputStreamPreferences(std::string pStreamCodec, bool pRtpActivated, bool pDoReset)
{
    bool tResult = false;
    enum AVCodecID tStreamCodecId = GetCodecIDFromGuiName(pStreamCodec);

    if ((mSourceCodecId != tStreamCodecId) || (mRtpActivated != pRtpActivated))
    {
        LOG(LOG_VERBOSE, "Setting new input streaming preferences");

        tResult = true;

        // set new codec
        LOG(LOG_VERBOSE, "    ..stream codec: %d => %d (%s)", mSourceCodecId, tStreamCodecId, pStreamCodec.c_str());
        mSourceCodecId = tStreamCodecId;
        mRtpActivated = pRtpActivated;
        mRtpSourceCodecIdHint = tStreamCodecId;

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

void MediaSourceMem::SetFrameBufferPreBufferingTime(float pTime)
{
    if (pTime > MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME - 0.5)
        pTime = MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME - 0.5;
    if (pTime < 0)
        pTime = 0;

    MediaSource::SetFrameBufferPreBufferingTime(pTime);
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

    // check if we have a suggestion from RTP parser
    if (mRtpSourceCodecIdHint != AV_CODEC_ID_NONE)
        mSourceCodecId = mRtpSourceCodecIdHint;

    // there is no differentiation between H.263+ and H.263 when decoding an incoming video stream
    if (mSourceCodecId == AV_CODEC_ID_H263P)
        mSourceCodecId = AV_CODEC_ID_H263;

    // get a format description
    if (!DescribeInput(mSourceCodecId, &tFormat))
        return false;

    // build corresponding "AVIOContext"
    CreateIOContext(mStreamPacketBuffer, MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE, GetNextInputFrame, NULL, this, &tIoContext);

    // packet account is done within GetNextInputFrame()
    mDecoderThreadAcountsPackets = false;

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

    if (mFormatContext->start_time > 0)
    {
        LOG(LOG_WARN, "Found a start time of: %"PRId64", will assume 0 instead", mFormatContext->start_time);
        mFormatContext->start_time = 0;
    }

    // finds and opens the correct decoder
    if (!OpenDecoder())
        return false;

    // overwrite FPS by the playout FPS value
    mInputFrameRate = mOutputFrameRate;

    MarkOpenGrabDeviceSuccessful();

    if (!mGrabbingStopped)
        StartDecoder();

    // enforce pre-buffering for the first frame
    mDecoderRecalibrateRTGrabbingAfterSeeking = (mDecoderFramePreBufferTime > 0);

    return true;
}

bool MediaSourceMem::OpenAudioGrabDevice(int pSampleRate, int pChannels)
{
    AVIOContext           *tIoContext;
    AVInputFormat       *tFormat;

    mMediaType = MEDIA_AUDIO;
    mOutputAudioChannels = pChannels;
    mOutputAudioSampleRate = pSampleRate;
    mOutputAudioFormat = AV_SAMPLE_FMT_S16; // assume we always want signed 16 bit

    LOG(LOG_VERBOSE, "Trying to open the audio source");

    SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(MEM)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);

    // check if we have a suggestion from RTP parser
    if (mRtpSourceCodecIdHint != AV_CODEC_ID_NONE)
        mSourceCodecId = mRtpSourceCodecIdHint;

    // get a format description
    if (!DescribeInput(mSourceCodecId, &tFormat))
        return false;

    // build corresponding "AVIOContext"
    CreateIOContext(mStreamPacketBuffer, MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE, GetNextInputFrame, NULL, this, &tIoContext);

    // packet account is done within GetNextInputFrame()
    mDecoderThreadAcountsPackets = false;

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

    if (mFormatContext->start_time > 0)
    {
        LOG(LOG_WARN, "Found a start time of: %"PRId64", will assume 0 instead", mFormatContext->start_time);
        mFormatContext->start_time = 0;
    }

    // ffmpeg might have difficulties detecting the correct input format, enforce correct audio parameters
    AVCodecContext *tCodec = mMediaStream->codec;
    if (mRtpActivated)
    {
        LOG(LOG_VERBOSE, "Setting time base for %s %s RTP stream with codec %s", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), HM_avcodec_get_name(mSourceCodecId));
        switch(mSourceCodecId)
        {
            case AV_CODEC_ID_AMR_NB:
                tCodec->channels = 1;
                tCodec->bit_rate = 7950;
                tCodec->sample_rate = 8000;
                mMediaStream->time_base.den = tCodec->sample_rate;
                mMediaStream->time_base.num = 1;
                break;
            case AV_CODEC_ID_ADPCM_G722:
                tCodec->channels = 1;
                tCodec->sample_rate = 16000;
                mMediaStream->time_base.den = 8000; // different time base as defined in RFC
                mMediaStream->time_base.num = 1;
                break;
            case AV_CODEC_ID_GSM:
            case AV_CODEC_ID_PCM_ALAW:
            case AV_CODEC_ID_PCM_MULAW:
                tCodec->channels = 1;
                tCodec->sample_rate = 8000;
                mMediaStream->time_base.den = tCodec->sample_rate;
                mMediaStream->time_base.num = 1;
                break;
            case AV_CODEC_ID_PCM_S16BE:
                tCodec->channels = 2;
                tCodec->sample_rate = 44100;
                mMediaStream->time_base.den = tCodec->sample_rate;
                mMediaStream->time_base.num = 1;
                break;
            case AV_CODEC_ID_MP3:
                mMediaStream->time_base.den = tCodec->sample_rate;
                mMediaStream->time_base.num = 1;
                break;
            default:
                mMediaStream->time_base.den = tCodec->sample_rate;
                mMediaStream->time_base.num = 1;
                break;
        }
    }

    // finds and opens the correct decoder
    if (!OpenDecoder())
        return false;

    mInputFrameRate = (float)mInputAudioSampleRate / mCodecContext->frame_size /* shouldn't be zero here */;

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

    mCurrentOutputFrameIndex = -1;
    mLastBufferedOutputFrameIndex = 0;
    mLastTimeWaitForRTGrabbing = 0;
    mTimeLastWrittenOutputChunk = 0;
    mSourceTimeShiftForRTGrabbing = 0;
    mDecoderSinglePictureGrabbed = false;
    mEOFReached = false;
    mDecoderRecalibrateRTGrabbingAfterSeeking = true;
    mResXLastGrabbedFrame = 0;
    mResYLastGrabbedFrame = 0;
    mMetaData.clear();

    // reinit. RTP parser
    RTP::Init();

    return tResult;
}

bool MediaSourceMem::IsSeeking()
{
    return (mDecoderTargetOutputFrameIndex > 0);
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
    int64_t tGrabStartTime = Time::GetTimeStamp();

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

        int tAvailableFrames = (mDecoderFifo != NULL) ? mDecoderFifo->GetUsage() : -1;

        // missing input?
        if (tAvailableFrames == 0)
        {
            // EOF reached?
            if (mEOFReached)
            {// queue empty and EOF reached
                mFrameNumber++;

                // unlock grabbing
                mGrabMutex.unlock();

                LOG(LOG_VERBOSE, "No %s frame in FIFO available and EOF marker is active", GetMediaTypeStr().c_str());

                // acknowledge "success"
                MarkGrabChunkSuccessful(mFrameNumber); // don't panic, it is only EOF

                LOG(LOG_WARN, "%s-Grabber reached EOF", GetMediaTypeStr().c_str());

                // we have reached the end, there is no need to wait for an explicit frame
                mDecoderTargetOutputFrameIndex = 0;

                return GRAB_RES_EOF;
            }else
            {// queue empty but EOF not reached
                if ((!mDecoderWaitForNextKeyFrame) && (!mDecoderWaitForNextKeyFramePackets))
                {// okay, we want more output from the decoder thread
                    if (GetSourceType() == SOURCE_FILE)
                    {// source is a file
                        if (!IsSeeking())
                        {// we aren't waiting for a special frame
                            LOG(LOG_WARN, "System too slow?, %s %s grabber detected a buffer underrun", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());
                        }
                    }else
                    {// source is memory/network
                        //
                    }
                }else
                {// we have to wait until the decoder thread has new data after we triggered a seeking process
                    // nothing to complain about
                }
            }
        }

        // should we restart pre-buffering?
        if ((mDecoderFramePreBufferingAutoRestart) && (mDecoderFramePreBufferTime > 0) && (mDecoderFrameBufferTime < MEDIA_SOURCE_MEM_DEFAULT_E2E_DELAY_JITER))
        {// time to restart pre-buffering
            LOG(LOG_VERBOSE, "Pre-buffering will be restarted now..");

            // pretend that we had a seeking step and have to recalibrate the RT grabbing
            #ifdef MSMEM_DEBUG_PRE_BUFFERING
                LOG(LOG_WARN, "GrabChunk()-Triggering RT-Grabbing calibration");
            #endif
            mDecoderRecalibrateRTGrabbingAfterSeeking = true;
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
                mDecoderNeedWorkCondition.Signal();
                mDecoderNeedWorkConditionMutex.unlock();
                #ifdef MSMEM_DEBUG_DECODER_STATE
                    LOG(LOG_VERBOSE, "Signaling to decoder done");
                #endif
            }

            ReadOutputChunk((char*)pChunkBuffer, pChunkSize, tCurrentFramePts);
            #ifdef MSMEM_DEBUG_PACKETS
                LOG(LOG_VERBOSE, "Setting current %s frame index to %"PRId64, GetMediaTypeStr().c_str(), tCurrentFramePts);
                LOG(LOG_VERBOSE, "Remaining buffered frames in decoder FIFO: %d", tAvailableFrames);
            #endif
            mCurrentOutputFrameIndex = tCurrentFramePts;
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

            LOG(LOG_WARN, "Signaling invalid %s grabber result", GetMediaTypeStr().c_str());
            return GRAB_RES_INVALID;
        }

        #ifdef MSMEM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Grabbed chunk %d of size %d with pts %"PRId64" from decoder FIFO", mFrameNumber, pChunkSize, tCurrentFramePts);
        #endif

        if (IsSeeking())
        {
            if (tCurrentFramePts < mDecoderTargetOutputFrameIndex)
            {// we are waiting for some special frame number
                LOG(LOG_VERBOSE, "Dropping grabbed %s frame %"PRId64" because we are still waiting for frame %.2lf", GetMediaTypeStr().c_str(), tCurrentFramePts, mDecoderTargetOutputFrameIndex);
                tShouldGrabNext = true;
            }else
            {
                // reset seeking flag because we have found the desired frame
                mDecoderTargetOutputFrameIndex = 0;
            }
        }

        // check for EOF
        int64_t tCurrentRelativeFramePts = tCurrentFramePts - CalculateOutputFrameNumber(mInputStartPts);
        if ((tCurrentRelativeFramePts >= mNumberOfFrames) && (mNumberOfFrames != 0) && (!InputIsPicture()))
        {// PTS value is bigger than possible max. value, EOF reached
            LOG(LOG_VERBOSE, "%s PTS value %"PRId64" (%"PRId64" - %.2lf) is bigger than or equal to maximum %.2lf", GetMediaTypeStr().c_str(), tCurrentRelativeFramePts, tCurrentFramePts, mInputStartPts, mNumberOfFrames);

            //no panic, ignore this and continue playback
        }

        // unlock grabbing
        mGrabMutex.unlock();

        #ifdef MSMEM_DEBUG_PACKETS
            if (tShouldGrabNext)
                LOG(LOG_VERBOSE, "Have to grab another frame, loop: %d", ++tGrabLoops);
        #endif

        // #########################################
        // frame rate emulation
        // #########################################
        // inspired by "output_packet" from ffmpeg.c
        if (mGrabberProvidesRTGrabbing)
        {
            if (!tShouldGrabNext)
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
                        LOG(LOG_VERBOSE, "Read valid %s frame %.2f after seeking in input stream", GetMediaTypeStr().c_str(), (float)mCurrentOutputFrameIndex);
                    #endif

                    mDecoderRecalibrateRTGrabbingAfterSeeking = false;
                }

                // RT grabbing - do we have to wait?
                if (!WaitForRTGrabbing())
                {
                    LOG(LOG_WARN, "%s frame from %s source is too late, frame dropped", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());
                    tShouldGrabNext = true;
                }
            }
        }
        if ((tShouldGrabNext) && (Time::GetTimeStamp() >= tGrabStartTime + MEDIA_SOURCE_MEM_GRABBING_TIMEOUT * AV_TIME_BASE))
        {
            LOG(LOG_VERBOSE, "Timeout of %.2f seconds occurred for %s grabbing, haven't found a suitable frame, using the current one anyhow", (float)MEDIA_SOURCE_MEM_GRABBING_TIMEOUT, GetMediaTypeStr().c_str());
            tShouldGrabNext = false;
        }
    }while (tShouldGrabNext);

    // acknowledge success
    MarkGrabChunkSuccessful(mFrameNumber);

    return mFrameNumber;
}

bool MediaSourceMem::InputIsPicture()
{
    bool tResult = false;

    // do we have a picture?
    if ((mMediaSourceOpened) &&
        (mFormatContext != NULL) && (mMediaStream) &&
        (mMediaStream->codec->codec_type == AVMEDIA_TYPE_VIDEO) &&
        (mMediaStream->duration == 1))
        tResult = true;

    return tResult;
}

void MediaSourceMem::StartDecoder()
{
    LOG(LOG_VERBOSE, "Starting %s decoder with FIFO", GetMediaTypeStr().c_str());

    mDecoderThreadNeeded = false;

    if (!IsRunning())
    {
        // start decoder main loop
        StartThread();

        int tLoops = 0;

        // wait until thread is running
        while ((!IsRunning() /* wait until thread is started */) || (!mDecoderThreadNeeded /* wait until thread has finished the init. process */))
        {
            if (tLoops % 10 == 0)
                LOG(LOG_VERBOSE, "Waiting for start of %s decoding thread, loop count: %d", GetMediaTypeStr().c_str(), ++tLoops);
            Thread::Suspend(25 * 1000);
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
        mDecoderThreadNeeded = false;

        // wait for termination of decoder thread
        do
        {
            if(tSignalingRound > 0)
                LOG(LOG_WARN, "Signaling attempt %d to stop %s decoder", tSignalingRound, GetMediaTypeStr().c_str());
            tSignalingRound++;

            WriteFragment(tmp, 0, 0);

            // force a wake up of decoder thread
            mDecoderNeedWorkCondition.Signal();

            Suspend(25 * 1000);
        }while(IsRunning());
    }
    mDecoderFragmentFifoDestructionMutex.unlock();

    LOG(LOG_VERBOSE, "Decoder stopped");
}

VideoScaler* MediaSourceMem::CreateVideoScaler()
{
    VideoScaler *tResult;

    LOG(LOG_VERBOSE, "Starting video scaler thread..");
    tResult = new VideoScaler(this, "Video-Decoder(" + GetSourceTypeStr() + ")");
    if(tResult == NULL)
        LOG(LOG_ERROR, "Invalid video scaler instance, possible out of memory");
    LOG(LOG_VERBOSE, "Starting video scaler with queue size %d (%f, %f)", CalculateFrameBufferSize(), mDecoderFrameBufferTimeMax, mOutputFrameRate);
    tResult->StartScaler(CalculateFrameBufferSize(), mSourceResX, mSourceResY, mCodecContext->pix_fmt, mTargetResX, mTargetResY, PIX_FMT_RGB32);

    return tResult;
}

void MediaSourceMem::CloseVideoScaler(VideoScaler *pScaler)
{
    pScaler->StopScaler();
}

void MediaSourceMem::ReadFrameFromInputStream(AVPacket *pPacket, double &pFrameTimestamp)
{
    int             tRes;
    int             tReadLoop = 0;

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
            if (pPacket->data != NULL)
                av_free_packet(pPacket);
        }

        // reset packet
        pPacket->data = NULL;
        pPacket->size = 0;

        tReadLoop++;

        // #########################################
        // reset time reference and let the packet
        // receiver determine a new one for the next
        // frame
        // #########################################
        mFirstReceivedFrameTimestampFromRTP = -1;

        // #########################################
        // read next sample from source - BLOCKING
        // #########################################
        tRes = av_read_frame(mFormatContext, pPacket);
        if (tRes < 0)
        {// failed to read frame
            #ifdef MSMEM_DEBUG_PACKETS
                if (!InputIsPicture())
                    LOG(LOG_VERBOSE, "Read bad frame %d with %d bytes from stream %d and result %s(%d)", pPacket->pts, pPacket->size, pPacket->stream_index, strerror(AVUNERROR(tRes)), tRes);
                else
                    LOG(LOG_VERBOSE, "Read bad picture %d with %d bytes from stream %d and result %s(%d)", pPacket->pts, pPacket->size, pPacket->stream_index, strerror(AVUNERROR(tRes)), tRes);
            #endif

            // error reporting
            if (!mGrabbingStopped)
            {
                if ((tRes == (int)AVUNERROR(ENODEV)) || (HasInputStreamChanged()))
                {
                    LOG(LOG_WARN, "%s-Decoder reached EOF because device isn't available anymore", GetMediaTypeStr().c_str());
                    mEOFReached = true;
                }else if (tRes == (int)AVERROR_EOF)
                {
                    pFrameTimestamp = mNumberOfFrames;
                    LOG(LOG_WARN, "%s-Decoder reached EOF", GetMediaTypeStr().c_str());
                    mEOFReached = true;
                }else if (tRes == (int)AVUNERROR(EIO))
                {
                    // acknowledge failed"
                    MarkGrabChunkFailed(GetMediaTypeStr() + " source has I/O error");

                    // signal EOF instead of I/O error
                    LOG(LOG_VERBOSE, "Returning EOF in %s stream because of I/O error", GetMediaTypeStr().c_str());
                    mEOFReached = true;
                }else if (tRes == (int)AVUNERROR(ENOMEM))
                {
                    // acknowledge failed"
                    MarkGrabChunkFailed(GetMediaTypeStr() + " source has out-of-memory");

                    // signal EOF instead of I/O error
                    LOG(LOG_VERBOSE, "Returning EOF in %s stream because of out-of-memory", GetMediaTypeStr().c_str());
                    mEOFReached = true;
                }else if (tRes != (int)AVUNERROR(EAGAIN))
                {// we should grab again, we signaled this ourself
                    if (mDecoderThreadNeeded)
                        tShouldReadNext = true;
                }else
                {// actually an error
                    LOG(LOG_ERROR, "Couldn't grab a %s frame because \"%s\"(%d)", GetMediaTypeStr().c_str(), strerror(AVUNERROR(tRes)), tRes);
                }
            }
        }else
        {// new frame was read
            if (pPacket->stream_index == mMediaStreamIndex)
            {
                #ifdef MSMEM_DEBUG_PACKET_TIMING
                    if (!InputIsPicture())
                        LOG(LOG_VERBOSE, "Read good frame %ld with %ld bytes from stream %d", pPacket->pts, pPacket->size, pPacket->stream_index);
                    else
                        LOG(LOG_VERBOSE, "Read good picture %ld with %ld bytes from stream %d", pPacket->pts, pPacket->size, pPacket->stream_index);
                #endif

                // is "presentation timestamp" stored within media stream?
                if (pPacket->dts != (int64_t)AV_NOPTS_VALUE)
                { // DTS value
                    pFrameTimestamp = pPacket->dts;
                    #ifdef MSMEM_DEBUG_PACKET_TIMING
                        LOG(LOG_VERBOSE, "Read frame with %d bytes of %s data with DTS: %ld => %.2f %lf", pPacket->size, GetMediaTypeStr().c_str(), pPacket->dts, (float)pFrameTimestamp, av_q2d(mMediaStream->time_base));
                    #endif
                    #ifdef MSMEM_DEBUG_PACKETS
                        if ((pPacket->dts < mDecoderLastReadPts) && (mDecoderLastReadPts != 0) && (pPacket->dts > 16 /* ignore the first frames */))
                            LOG(LOG_VERBOSE, "%s-DTS values are non continuous in stream, read DTS: %"PRId64", last %.2lf, alternative PTS: %"PRId64, GetMediaTypeStr().c_str(), pPacket->dts, mDecoderLastReadPts, pPacket->pts);
                    #endif
                }else
                {// PTS value
                    pFrameTimestamp = pPacket->pts;
                    #ifdef MSMEM_DEBUG_PACKET_TIMING
                        LOG(LOG_VERBOSE, "Read frame with %d bytes with PTS: %ld => %.2f %lf", pPacket->size, pPacket->pts, (float)pFrameTimestamp, av_q2d(mMediaStream->time_base));
                    #endif
                    #ifdef MSMEM_DEBUG_PACKETS
                        if ((pPacket->pts < mDecoderLastReadPts) && (mDecoderLastReadPts != 0) && (pPacket->pts > 16 /* ignore the first frames */))
                            LOG(LOG_VERBOSE, "%s-PTS values are non continuous in stream, %"PRId64" is lower than last %.2lf, alternative DTS: %"PRId64", difference is: %"PRId64, GetMediaTypeStr().c_str(), pPacket->pts, mDecoderLastReadPts, pPacket->dts, mDecoderLastReadPts - pPacket->pts);
                    #endif
                }

                if (mRtpActivated)
                {// derive the frame number from the RTP timestamp, which works independent from packet loss
                    pFrameTimestamp = CalculateFrameNumberFromRTP();
                    int64_t tPacketPts = rint(pFrameTimestamp);
                    #ifdef MSMEM_DEBUG_PACKET_TIMING
                        LOG(LOG_VERBOSE, "Read frame with %d bytes with PTS: %ld => %ld %lf", pPacket->size, pPacket->pts, tPacketPts, av_q2d(mMediaStream->time_base));
                    #endif
                    pPacket->pts = tPacketPts;
                    pPacket->dts = tPacketPts;
                    #ifdef MSMEM_DEBUG_PRE_BUFFERING
                        LOG(LOG_VERBOSE, "Got from RTP data the frame number: %d", pFrameTimestamp);
                    #endif
                }

                // for seeking: is the currently read frame close to target frame index?
                //HINT: we need a key frame in the remaining distance to the target frame)
                if ((IsSeeking()) && (CalculateOutputFrameNumber(pFrameTimestamp) < mDecoderTargetOutputFrameIndex - MEDIA_SOURCE_MEM_SEEK_MAX_EXPECTED_GOP_SIZE))
                {// we are still waiting for a special frame number
                    #ifdef MSMEM_DEBUG_SEEKING
                        LOG(LOG_VERBOSE, "Dropping %s frame %"PRId64" because we are waiting for frame %.2f", GetMediaTypeStr().c_str(), pFrameTimestamp, mDecoderTargetOutputFrameIndex);
                    #endif
                    tShouldReadNext = true;
                }else
                {
                    if (mDecoderRecalibrateRTGrabbingAfterSeeking)
                    {
                        // wait for next key frame packets (either i-frame or p-frame
                        if (mDecoderWaitForNextKeyFramePackets)
                        {
                            if (pPacket->flags & AV_PKT_FLAG_KEY)
                            {
                                #ifdef MSMEM_DEBUG_SEEKING
                                    LOG(LOG_VERBOSE, "Read first %s key packet in packet frame number %"PRId64" with flags %d from input stream after seeking", GetMediaTypeStr().c_str(), pFrameTimestamp, pPacket->flags);
                                #endif
                                mDecoderWaitForNextKeyFramePackets = false;
                            }else
                            {
                                #ifdef MSMEM_DEBUG_SEEKING
                                    LOG(LOG_VERBOSE, "Dropping %s frame packet %"PRId64" because we are waiting for next key frame packets after seek target frame %.2f", GetMediaTypeStr().c_str(), pFrameTimestamp, mDecoderTargetOutputFrameIndex);
                                #endif
                               tShouldReadNext = true;
                            }
                        }
                        #ifdef MSMEM_DEBUG_SEEKING
                            LOG(LOG_VERBOSE, "Read %s frame number %"PRId64" from input stream after seeking", GetMediaTypeStr().c_str(), pFrameTimestamp);
                        #endif
                    }else
                    {
                        #if defined(MSMEM_DEBUG_DECODER_STATE) || defined(MSMEM_DEBUG_PACKETS)
                            LOG(LOG_WARN, "Read %s frame number %"PRId64" from input stream, last frame was %.2lf", GetMediaTypeStr().c_str(), pFrameTimestamp, mDecoderLastReadPts);
                        #endif
                    }
                }
                // check if PTS value is continuous
                if ((mDecoderLastReadPts > 0) && (pFrameTimestamp < mDecoderLastReadPts))
                {// current packet is older than the last packet
                    LOG(LOG_WARN, "Found interleaved %s packets in %s source, non continuous PTS: %.2lf => %.2lf", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), mDecoderLastReadPts, pFrameTimestamp);
                }
                mDecoderLastReadPts = pFrameTimestamp;
            }else
            {
                if (pPacket->stream_index < 0)
                {
                    LOG(LOG_ERROR, "Read a packet with invalid stream index: %d", pPacket->stream_index);
                }

                tShouldReadNext = true;
                if (mRtpActivated)
                {
                    LOG(LOG_ERROR, "Read %s frame %d of stream %d instead of desired stream %d, this should never happen in case of a single RTP stream", GetMediaTypeStr().c_str(), pPacket->pts, pPacket->stream_index, mMediaStreamIndex);
                }else
                {
                    #ifdef MSMEM_DEBUG_PACKETS
                        LOG(LOG_VERBOSE, "Read %s frame %d of stream %d instead of desired stream %d", GetMediaTypeStr().c_str(), pPacket->pts, pPacket->stream_index, mMediaStreamIndex);
                    #endif
                }
            }
        }
    }while ((tShouldReadNext) && (!mEOFReached) && (mDecoderThreadNeeded));

    if (mGrabbingStopped)
    {
        LOG(LOG_VERBOSE, "%s %s grabbing was stopped while ReadFrameFromInputStream() was running", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());
    }
    #ifdef MSMEM_DEBUG_PACKETS
        if (tReadIteration > 1)
            LOG(LOG_VERBOSE, "Needed %d read iterations to get next %s  packet from source stream", tReadIteration, GetMediaTypeStr().c_str());
        //LOG(LOG_VERBOSE, "New %s chunk with size: %d and stream index: %d", GetMediaTypeStr().c_str(), pPacket->size, pPacket->stream_index);
    #endif
}

bool MediaSourceMem::IsAcceptableStartFrame(AVFrame *pFrame)
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

//###################################
//### Decoder thread
//###################################
void* MediaSourceMem::Run(void* pArgs)
{
    bool                tAlreadyWarnedThatFrameSizeDiffers = false;
    AVFrame             *tVideoSourceFrame = NULL;
    AVPacket            tPacketStruc, *tPacket = &tPacketStruc;
    int                 tFrameFinished = 0;
    int                 tDecoderResult = 0;
    int                 tRes = 0;
    int                 tChunkBufferSize = 0;
    uint8_t             *tChunkBuffer;
    int                 tWaitLoop;
    bool                tInputIsPicture = false;
    /* current chunk */
    int                 tCurrentChunkSize = 0;
    double              tCurrentInputFrameTimestamp = 0; // input PTS/DTS from packets
    double              tCurrentOutputFrameTimestamp = 0; // input PTS/DTS from packets - but correct by ffmpeg, used for output
    double              tCurrentOutputFrameNumber = 0; // output frame number from decoder
    /* video scaler */
    VideoScaler         *tVideoScaler = NULL;
    /* picture as input */
    AVFrame             *tVideoPictureFrame = NULL;
    /* audio */
    AVFifoBuffer        *tSampleFifo = NULL;
    AVFrame             *tAudioFrame = NULL;

    // reset EOF marker
    mEOFReached = false;

    tInputIsPicture = InputIsPicture();

    LOG(LOG_WARN, ">>>>>>>>>>>>>>>> %s-Decoding thread for %s media source started", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());

    if (mDecoderFifo != NULL)
    {
        LOG(LOG_VERBOSE, "Releasing old %s decoder FIFO", GetMediaTypeStr().c_str());

        // make sure the grabber isn't active at the moment
        delete mDecoderFifo;
        mDecoderFifo = NULL;

        LOG(LOG_VERBOSE, "%s decoder FIFO released", GetMediaTypeStr().c_str());
    }

    switch(mMediaType)
    {
        case MEDIA_VIDEO:
            SVC_PROCESS_STATISTIC.AssignThreadName("Video-Decoder(" + GetSourceTypeStr() + ")");

            // Allocate video frame for YUV format
            if ((tVideoSourceFrame = AllocFrame()) == NULL)
                LOG(LOG_ERROR, "Out of video memory");

            if (!tInputIsPicture)
            {// we have a video stream as input
                LOG(LOG_VERBOSE, "Allocating all objects for a video input");

                tChunkBufferSize = avpicture_get_size(mCodecContext->pix_fmt, mSourceResX, mSourceResY) + FF_INPUT_BUFFER_PADDING_SIZE;

                // allocate chunk buffer
                tChunkBuffer = (uint8_t*)av_malloc(tChunkBufferSize);

                // create video scaler
                tVideoScaler = CreateVideoScaler();

                // set the video scaler as FIFO for the decoder
                mDecoderFifo = tVideoScaler;
            }else
            {// we have single frame (a picture) as input
                LOG(LOG_VERBOSE, "Allocating all objects for a picture input");

                tChunkBufferSize = avpicture_get_size(PIX_FMT_RGB32, mTargetResX, mTargetResY) + FF_INPUT_BUFFER_PADDING_SIZE;

                // allocate chunk buffer
                tChunkBuffer = (uint8_t*)av_malloc(tChunkBufferSize);

                LOG(LOG_VERBOSE, "Decoder thread does not need the scaler thread because input is a picture");

                // Allocate video frame for RGB format
                if ((tVideoPictureFrame = AllocFrame()) == NULL)
                    LOG(LOG_ERROR, "Out of video memory");

                // Assign appropriate parts of buffer to image planes in tVideoPictureFrame
                avpicture_fill((AVPicture *)tVideoPictureFrame, (uint8_t *)tChunkBuffer, PIX_FMT_RGB32, mTargetResX, mTargetResY);

                LOG(LOG_VERBOSE, "Creating %s media FIFO with %d entries of %d bytes", GetMediaTypeStr().c_str(), CalculateFrameBufferSize(), tChunkBufferSize);
                mDecoderFifo = new MediaFifo(CalculateFrameBufferSize(), tChunkBufferSize, GetMediaTypeStr() + "-MediaSource" + GetSourceTypeStr());
            }

            break;
        case MEDIA_AUDIO:
            SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Decoder(" + GetSourceTypeStr() + ")");

            // Allocate video frame for YUV format
             if ((tAudioFrame = AllocFrame()) == NULL)
                 LOG(LOG_ERROR, "Out of audio memory");

            tChunkBufferSize = AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE;

            // allocate chunk buffer
            tChunkBuffer = (uint8_t*)av_malloc(tChunkBufferSize);

            LOG(LOG_VERBOSE, "Creating %s media FIFO with %d entries of %d bytes", GetMediaTypeStr().c_str(), CalculateFrameBufferSize(), tChunkBufferSize);
            mDecoderFifo = new MediaFifo(CalculateFrameBufferSize(), tChunkBufferSize, GetMediaTypeStr() + "-MediaSource" + GetSourceTypeStr());

            break;
        default:
            SVC_PROCESS_STATISTIC.AssignThreadName("Decoder(" + GetSourceTypeStr() + ")");
            LOG(LOG_ERROR, "Unknown media type");
            return NULL;
            break;
    }

    if (!OpenFormatConverter())
        LOG(LOG_ERROR, "Failed to open the %s format converter", GetMediaTypeStr().c_str());

    // reset some state variables
    mDecoderLastReadPts = 0;
    mCurrentOutputFrameIndex = -1;
    mLastBufferedOutputFrameIndex = 0;
    mLastTimeWaitForRTGrabbing = 0;
    mTimeLastWrittenOutputChunk = 0;

    // signal that decoder thread has finished init.
    mDecoderThreadNeeded = true;

    CalculateExpectedOutputPerInputFrame();

    // trigger an avcodec_flush_buffers()
    ResetDecoderBuffers();

    LOG(LOG_WARN, "================ Entering main %s decoding loop for %s media source", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());

    while (mDecoderThreadNeeded)
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
                mDecoderSeekMutex.lock();
                ReadFrameFromInputStream(tPacket, tCurrentInputFrameTimestamp);
                //TODO: LOG(LOG_VERBOSE, "%s: %.2f %.2f", GetMediaTypeStr().c_str(), (float)tCurrentInputFrameTimestamp, (float)(tCurrentInputFrameTimestamp - mMediaStream->start_time));
                mDecoderSeekMutex.unlock();
            }else
            {// no packet was read
                //LOG(LOG_VERBOSE, "No packet was read");

                // generate dummy packet (av_free_packet() will destroy it later)
                av_init_packet(tPacket);
            }

            // #########################################
            // start packet processing
            // #########################################
            if (((tPacket->data != NULL) && (tPacket->size > 0)) || (mDecoderSinglePictureGrabbed /* we already grabbed the single frame from the picture input */))
            {
                if (mFormatContext->iformat->flags & AVFMT_TS_DISCONT)
                {
//TODO: deactivated again because it leads to problems with VOB files
//                    if ((tPacket->duration != mFrameDuration) && (tPacket->duration> 0))
//                    {
//                        LOG(LOG_WARN, "Detected new packet duration, changing the frame timestamp factor from %d to %d", mFrameDuration, tPacket->duration);
//                        mFrameDuration = tPacket->duration;
//                    }
                }

                #ifdef MSMEM_DEBUG_PACKET_RECEIVER
                    if ((tPacket->data != NULL) && (tPacket->size > 0))
                    {
                        LOG(LOG_VERBOSE, "New %s packet..", GetMediaTypeStr().c_str());
                        LOG(LOG_VERBOSE, "      ..duration: %d", tPacket->duration);
                        LOG(LOG_VERBOSE, "      ..pts: %"PRId64", normalized pts: %lld, start time: %"PRId64, tPacket->pts, tPacket->pts - mMediaStream->start_time, mMediaStream->start_time);
                        LOG(LOG_VERBOSE, "      ..stream: %"PRId64, tPacket->stream_index);
                        LOG(LOG_VERBOSE, "      ..dts: %"PRId64, tPacket->dts);
                        LOG(LOG_VERBOSE, "      ..size: %d", tPacket->size);
                        LOG(LOG_VERBOSE, "      ..pos: %"PRId64, tPacket->pos);
                        LOG(LOG_VERBOSE, "      ..frame number: %"PRId64, (tPacket->pts - mMediaStream->start_time) / (tPacket->duration > 0 ? tPacket->duration : 1) /* works only for cfr, otherwise duration is variable */);
                        LOG(LOG_VERBOSE, "      ..current input frame timestamp: %lf", tCurrentInputFrameTimestamp);
                        if (tPacket->flags == AV_PKT_FLAG_KEY)
                            LOG(LOG_VERBOSE, "      ..flags: key frame");
                        else
                            LOG(LOG_VERBOSE, "      ..flags: %d", tPacket->flags);
                    }
                #endif

                //LOG(LOG_VERBOSE, "New %s packet: dts: %"PRId64", pts: %"PRId64", pos: %"PRId64", duration: %d", GetMediaTypeStr().c_str(), tPacket->dts, tPacket->pts, tPacket->pos, tPacket->duration);

                // #########################################
                // process packet
                // #########################################
                mDecoderResetBuffersMutex.lock();
                tCurrentChunkSize = 0;
                switch(mMediaType)
                {
                    case MEDIA_VIDEO:
                        {
                            if ((!tInputIsPicture) || (!mDecoderSinglePictureGrabbed))
                            {// we try to decode packet(s) from input stream -> either the desired picture or a single frame from the stream
                                // log statistics
                                if (mDecoderThreadAcountsPackets)
                                    AnnouncePacket(tPacket->size);
                                #ifdef MSMEM_DEBUG_PACKETS
                                    LOG(LOG_VERBOSE, "Decoding video frame (input is picture: %d)..", tInputIsPicture);
                                #endif

                                // did we read the single frame of a picture?
                                if ((tInputIsPicture) && (!mDecoderSinglePictureGrabbed))
                                {// store it
                                    LOG(LOG_VERBOSE, "Found picture packet of size %d, pts: %"PRId64", dts: %"PRId64" and store it in the picture buffer", tPacket->size, tPacket->pts, tPacket->dts);
                                    mDecoderSinglePictureGrabbed = true;
                                    mEOFReached = false;
                                }

                                // ############################
                                // ### DECODE FRAME
                                // ############################
                                tFrameFinished = 0;
                                tDecoderResult = HM_avcodec_decode_video(mCodecContext, tVideoSourceFrame, &tFrameFinished, tPacket);

                                #ifdef MSMEM_DEBUG_VIDEO_FRAME_RECEIVER
                                    LOG(LOG_VERBOSE, "New video frame before PTS adaption..");
                                    LOG(LOG_VERBOSE, "      ..key frame: %d", tVideoSourceFrame->key_frame);
                                    LOG(LOG_VERBOSE, "      ..picture type: %s-frame", GetFrameType(tVideoSourceFrame).c_str());
                                    LOG(LOG_VERBOSE, "      ..pts: %"PRId64, tVideoSourceFrame->pts);
                                    LOG(LOG_VERBOSE, "      ..pkt pts: %"PRId64, tVideoSourceFrame->pkt_pts);
                                    LOG(LOG_VERBOSE, "      ..pkt dts: %"PRId64, tVideoSourceFrame->pkt_dts);
//                                    LOG(LOG_VERBOSE, "      ..resolution: %d * %d", tVideoSourceFrame->width, tVideoSourceFrame->height);
//                                    LOG(LOG_VERBOSE, "      ..coded pic number: %d", tVideoSourceFrame->coded_picture_number);
//                                    LOG(LOG_VERBOSE, "      ..display pic number: %d", tVideoSourceFrame->display_picture_number);
                                #endif

                                // ############################
                                // ### store data planes and line size (for decoder loops >= 2)
                                // ############################
                                if ((tInputIsPicture) && (mDecoderSinglePictureGrabbed))
                                {
                                    for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
                                    {
                                        mDecoderSinglePictureData[i] = tVideoSourceFrame->data[i];
                                        mDecoderSinglePictureLineSize[i] = tVideoSourceFrame->linesize[i];
                                    }
                                }

                                //LOG(LOG_VERBOSE, "New %s source frame: dts: %"PRId64", pts: %"PRId64", pos: %"PRId64", pic. nr.: %d", GetMediaTypeStr().c_str(), tVideoSourceFrame->pkt_dts, tVideoSourceFrame->pkt_pts, tVideoSourceFrame->pkt_pos, tVideoSourceFrame->display_picture_number);

                                // MPEG2 picture repetition
                                if (tVideoSourceFrame->repeat_pict != 0)
                                    LOG(LOG_ERROR, "MPEG2 picture should be repeated for %d times, unsupported feature!", tVideoSourceFrame->repeat_pict);

                                #ifdef MSMEM_DEBUG_PACKETS
                                    LOG(LOG_VERBOSE, "    ..video decoding ended with result %d and %d bytes of output", tFrameFinished, tDecoderResult);
                                #endif

                                // log lost packets: difference between currently received frame number and the number of locally processed frames
                                SetLostPacketCount(tVideoSourceFrame->coded_picture_number - mFrameNumber);

                                #ifdef MSMEM_DEBUG_PACKETS
                                    LOG(LOG_VERBOSE, "Video frame %d decoded", tVideoSourceFrame->coded_picture_number);
                                #endif

                                // ############################
                                // ### check codec ID
                                // ############################
                                // do we have a video codec change at sender side?
                                if ((mSourceCodecId != 0) && (mSourceCodecId != AV_CODEC_ID_MPEG2TS /* in this case the MPEG2TS is only the transport and carries inside another codec */) && (mSourceCodecId != mCodecContext->codec_id))
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

                                // ############################
                                // ### check video resolution
                                // ############################
                                // check if video resolution has changed within input stream (at remode side for network streams!)
                                if ((mResXLastGrabbedFrame != mCodecContext->width) || (mResYLastGrabbedFrame != mCodecContext->height))
                                {
                                    if ((mSourceResX != mCodecContext->width) || (mSourceResY != mCodecContext->height))
                                    {
                                        LOG(LOG_WARN, "Video resolution change in input stream from %s source detected", GetSourceTypeStr().c_str());

                                        // let the video scaler update the (ffmpeg based) scaler context
                                        tVideoScaler->ChangeInputResolution(mCodecContext->width, mCodecContext->height);

                                        // free the old chunk buffer
                                        av_free(tChunkBuffer);

                                        // calculate a new chunk buffer size for the new source video resolution
                                        tChunkBufferSize = avpicture_get_size(mCodecContext->pix_fmt, mCodecContext->width, mCodecContext->height) + FF_INPUT_BUFFER_PADDING_SIZE;

                                        // allocate the new chunk buffer
                                        tChunkBuffer = (uint8_t*)av_malloc(tChunkBufferSize);

                                        LOG(LOG_INFO, "Video resolution changed from %d*%d to %d * %d", mSourceResX, mSourceResY, mCodecContext->width, mCodecContext->height);

                                        // set grabbing resolution to the resolution from the codec, which has automatically detected the new resolution
                                        mSourceResX = mCodecContext->width;
                                        mSourceResY = mCodecContext->height;
                                    }

                                    mResXLastGrabbedFrame = mCodecContext->width;
                                    mResYLastGrabbedFrame = mCodecContext->height;
                                }

                                // ############################
                                // ### calculate PTS value
                                // ############################
                                // save PTS value to deliver it later to the frame grabbing thread
                                if ((tVideoSourceFrame->pkt_dts != (int64_t)AV_NOPTS_VALUE) || (tVideoSourceFrame->pkt_pts != (int64_t)AV_NOPTS_VALUE))
                                {// use PTS/DTS
                                    tCurrentOutputFrameTimestamp = HM_av_frame_get_best_effort_timestamp(tVideoSourceFrame);
                                    #ifdef MSMEM_DEBUG_TIMING
                                        LOG(LOG_VERBOSE, "Setting current frame PTS to frame packet BE PTS: %"PRId64, tCurrentOutputFrameTimestamp);
                                    #endif
                                }else
                                {// fall back to packet's PTS value
                                    #ifdef MSMEM_DEBUG_TIMING
                                        LOG(LOG_VERBOSE, "Setting current frame PTS to packet PTS: %.2f", (float)tCurrentInputFrameTimestamp);
                                    #endif
                                    tCurrentOutputFrameTimestamp = tCurrentInputFrameTimestamp;
                                }

                                tCurrentOutputFrameNumber = CalculateOutputFrameNumber(tCurrentOutputFrameTimestamp);
                                //TODO: LOG(LOG_WARN, "%s: %ld %.2f => %.2f", GetMediaTypeStr().c_str(), tCurrentOutputFrameTimestamp, (float)tCurrentInputFrameTimestamp, (float)tCurrentOutputFrameNumber);

                                #ifdef MSMEM_DEBUG_TIMING
                                    if ((tVideoSourceFrame->pkt_pts != tVideoSourceFrame->pkt_dts) && (tVideoSourceFrame->pkt_pts != (int64_t)AV_NOPTS_VALUE) && (tVideoSourceFrame->pkt_dts != (int64_t)AV_NOPTS_VALUE))
                                        LOG(LOG_VERBOSE, "PTS(%"PRId64") and DTS(%"PRId64") differ after %s decoding step, using as PTS %"PRId64, tVideoSourceFrame->pkt_pts, tVideoSourceFrame->pkt_dts, GetMediaTypeStr().c_str(), tCurrentOutputFrameTimestamp);
                                #endif
                            }else
                            {// reuse the stored picture
                                // ############################
                                // ### restore data planes and line sizes
                                // ############################
                                #ifdef MSMEM_DEBUG_PACKETS
                                    LOG(LOG_VERBOSE, "Restoring the source frame's data planes and line sizes from the stored values");
                                #endif
                                for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
                                {
                                    tVideoSourceFrame->data[i] = mDecoderSinglePictureData[i];
                                    tVideoSourceFrame->linesize[i] = mDecoderSinglePictureLineSize[i];
                                    tVideoSourceFrame->pict_type = AV_PICTURE_TYPE_I;
                                }

                                // simulate a monotonous increasing PTS value
                                tCurrentInputFrameTimestamp += 1;

                                // prepare the PTS value which is later delivered to the grabbing thread
                                tCurrentOutputFrameNumber = CalculateOutputFrameNumber(tCurrentInputFrameTimestamp);

                                // simulate a successful decoding step
                                tFrameFinished = 1;
                                // some value above 0
                                tDecoderResult = INT_MAX;
                            }

                            // ############################
                            // ### process valid frame
                            // ############################
                            if (tDecoderResult >= 0)
                            {
                                if (tFrameFinished != 0)
                                {
                                    #ifdef MSMEM_DEBUG_VIDEO_FRAME_RECEIVER
                                        LOG(LOG_VERBOSE, "New video frame..");
                                        LOG(LOG_VERBOSE, "      ..key frame: %d", tVideoSourceFrame->key_frame);
                                        LOG(LOG_VERBOSE, "      ..picture type: %s-frame", GetFrameType(tVideoSourceFrame).c_str());
                                        LOG(LOG_VERBOSE, "      ..pts: %"PRId64", original PTS: %.2lf", tVideoSourceFrame->pts, tCurrentOutputFrameNumber);
                                        LOG(LOG_VERBOSE, "      ..pkt pts: %"PRId64, tVideoSourceFrame->pkt_pts);
                                        LOG(LOG_VERBOSE, "      ..pkt dts: %"PRId64, tVideoSourceFrame->pkt_dts);
//                                        LOG(LOG_VERBOSE, "      ..resolution: %d * %d", tVideoSourceFrame->width, tVideoSourceFrame->height);
//                                        LOG(LOG_VERBOSE, "      ..coded pic number: %d", tVideoSourceFrame->coded_picture_number);
//                                        LOG(LOG_VERBOSE, "      ..display pic number: %d", tVideoSourceFrame->display_picture_number);
//                                        LOG(LOG_VERBOSE, "      ..context delay: %d", mCodecContext->delay);
//                                        LOG(LOG_VERBOSE, "      ..context frame nr.: %d", mCodecContext->frame_number);
                                    #endif

                                    // use the PTS value from the packet stream because it refers to the original (without decoder delays) timing
                                    tVideoSourceFrame->pts = rint(tCurrentOutputFrameNumber);
                                    tVideoSourceFrame->coded_picture_number = rint(tCurrentOutputFrameNumber);
                                    tVideoSourceFrame->display_picture_number = rint(tCurrentOutputFrameNumber);

                                    // wait for next key frame packets (either an i-frame or a p-frame)
                                    if (mDecoderWaitForNextKeyFrame)
                                    {// we are still waiting for the next key frame after seeking in the input stream
                                        if (IsAcceptableStartFrame(tVideoSourceFrame))
                                        {
                                            mDecoderWaitForNextKeyFrame = false;
                                            mDecoderWaitForNextKeyFrameTimeout = 0;
                                        }else
                                        {
                                            if (av_gettime() > mDecoderWaitForNextKeyFrameTimeout)
                                            {
                                                LOG(LOG_WARN, "We haven't found a key frame in the input stream within a specified time, giving up, continuing anyways");
                                                mDecoderWaitForNextKeyFrame = false;
                                                mDecoderWaitForNextKeyFrameTimeout = 0;
                                            }
                                        }

                                        if (!mDecoderWaitForNextKeyFrame)
                                        {
                                            LOG(LOG_VERBOSE, "Read first %s key frame at frame number %.2lf with flags %d from input stream after seeking", GetMediaTypeStr().c_str(), tCurrentOutputFrameNumber, tPacket->flags);
                                        }else
                                        {
                                            #ifdef MSMEM_DEBUG_SEEKING
                                                LOG(LOG_VERBOSE, "Dropping %s frame %.2lf because we are waiting for next key frame after seeking", GetMediaTypeStr().c_str(), tCurrentOutputFrameNumber);
                                            #endif
                                            #ifdef MSMEM_DEBUG_FRAME_QUEUE
                                                LOG(LOG_VERBOSE, "No %s frame will be written to frame queue, we ware waiting for the next key frame, current frame type: %s, EOF: %d", GetMediaTypeStr().c_str(), GetFrameType(tVideoSourceFrame).c_str(), mEOFReached);
                                            #endif

                                        }
                                    }
                                    if (!mDecoderWaitForNextKeyFrame)
                                    {// we are not waiting for next key frame and can proceed as usual
                                        // ############################
                                        // ### ANNOUNCE FRAME (statistics)
                                        // ############################
                                        AnnounceFrame(tVideoSourceFrame);

                                        // ############################
                                        // ### RECORD FRAME
                                        // ############################
                                        // re-encode the frame and write it to file
                                        if (mRecording)
                                            RecordFrame(tVideoSourceFrame);

                                        // ############################
                                        // ### SCALE FRAME (CONVERT): is done inside a separate thread
                                        // ############################
                                        if (!tInputIsPicture)
                                        {// we decode one frame of a stream
                                            #ifdef MSMEM_DEBUG_PACKETS
                                                LOG(LOG_VERBOSE, "Scale (separate thread) video frame..");
                                                LOG(LOG_VERBOSE, "Video frame data: %p, %p, %p, %p", tVideoSourceFrame->data[0], tVideoSourceFrame->data[1], tVideoSourceFrame->data[2], tVideoSourceFrame->data[3]);
                                                LOG(LOG_VERBOSE, "Video frame line size: %d, %d, %d, %d", tVideoSourceFrame->linesize[0], tVideoSourceFrame->linesize[1], tVideoSourceFrame->linesize[2], tVideoSourceFrame->linesize[3]);
                                            #endif

                                            //LOG(LOG_VERBOSE, "New %s RGB frame: dts: %"PRId64", pts: %"PRId64", pos: %"PRId64", pic. nr.: %d", GetMediaTypeStr().c_str(), tVideoPictureFrame->pkt_dts, tVideoPictureFrame->pkt_pts, tVideoPictureFrame->pkt_pos, tVideoPictureFrame->display_picture_number);

                                            if ((tRes = avpicture_layout((AVPicture*)tVideoSourceFrame, mCodecContext->pix_fmt, mSourceResX, mSourceResY, tChunkBuffer, tChunkBufferSize)) < 0)
                                            {
                                                LOG(LOG_WARN, "Couldn't copy AVPicture/AVFrame pixel data into chunk buffer because \"%s\"(%d)", strerror(AVUNERROR(tRes)), tRes);
                                            }else
                                            {// everything is okay, we have the current frame in tChunkBuffer and can send the frame to the video scaler
                                                tCurrentChunkSize = avpicture_get_size(mCodecContext->pix_fmt, mSourceResX, mSourceResY);
                                            }
                                        }else
                                        {// we decode a picture
                                            if  ((mDecoderSinglePictureResX != mTargetResX) || (mDecoderSinglePictureResY != mTargetResY))
                                            {
                                                #ifdef MSMEM_DEBUG_PACKETS
                                                    LOG(LOG_VERBOSE, "Scale (within decoder thread) video frame..");
                                                    LOG(LOG_VERBOSE, "Video frame data: %p, %p", tVideoSourceFrame->data[0], tVideoSourceFrame->data[1]);
                                                    LOG(LOG_VERBOSE, "Video frame line size: %d, %d", tVideoSourceFrame->linesize[0], tVideoSourceFrame->linesize[1]);

                                                    // scale the video frame
                                                    LOG(LOG_VERBOSE, "Scaling video input picture..");
                                                #endif

                                                tRes = HM_sws_scale(mVideoScalerContext, tVideoSourceFrame->data, tVideoSourceFrame->linesize, 0, mCodecContext->height, tVideoPictureFrame->data, tVideoPictureFrame->linesize);
                                                if (tRes == 0)
                                                    LOG(LOG_ERROR, "Failed to scale the video frame");

                                                #ifdef MSMEM_DEBUG_PACKETS
                                                    LOG(LOG_VERBOSE, "Decoded picture into RGB frame: dts: %"PRId64", pts: %"PRId64", pos: %"PRId64", pic. nr.: %d", tVideoPictureFrame->pkt_dts, tVideoPictureFrame->pkt_pts, tVideoPictureFrame->pkt_pos, tVideoPictureFrame->display_picture_number);
                                                #endif

                                                mDecoderSinglePictureResX = mTargetResX;
                                                mDecoderSinglePictureResY = mTargetResY;
                                            }else
                                            {// use stored RGB frame

                                                //HINT: tCurremtChunkSize will be updated later
                                                //HINT: tChunkBuffer is still valid from first decoder loop
                                            }
                                            // return size of decoded frame
                                            tCurrentChunkSize = avpicture_get_size(PIX_FMT_RGB32, mTargetResX, mTargetResY);
                                        }

                                        // ############################
                                        // ### WRITE FRAME TO OUTPUT FIFO
                                        // ############################
                                        // add new chunk to FIFO
                                        if (tCurrentChunkSize <= mDecoderFifo->GetEntrySize())
                                        {
                                            #ifdef MSMEM_DEBUG_PACKETS
                                                LOG(LOG_VERBOSE, "Writing %d %s bytes at %p to FIFO with frame nr.%.2lf", tCurrentChunkSize, GetMediaTypeStr().c_str(), tChunkBuffer, tCurrentOutputFrameNumber);
                                            #endif
                                            WriteOutputChunk((char*)tChunkBuffer, tCurrentChunkSize, (int64_t)rint(tCurrentOutputFrameNumber));

                                            // prepare frame number for next loop
                                            tCurrentOutputFrameNumber += 1;

                                            #ifdef MSMEM_DEBUG_DECODER_STATE
                                                LOG(LOG_VERBOSE, "Successful audio buffer loop");
                                            #endif
                                        }else
                                        {
                                            LOG(LOG_ERROR, "Cannot write a %s chunk of %d bytes to the FIFO with %d bytes slots", GetMediaTypeStr().c_str(),  tCurrentChunkSize, mDecoderFifo->GetEntrySize());
                                        }
                                    }else
                                    {// still waiting for first key frame
                                        //nothing to do
                                    }
                                }else
                                {// tFrameFinished != 1
                                    LOG(LOG_WARN, "Video frame was buffered, FrameFinished: %d, codec context flags: 0x%X", tFrameFinished, mCodecContext->flags);
                                    if (tPacket->data != NULL)
                                        mDecoderOutputFrameDelay++;
                                }
                            }else
                            {// tDecoderResult < 0
                                LOG(LOG_ERROR, "Couldn't decode video frame %.2f because \"%s\"(%d), got a decoder result: %d", (float)tCurrentInputFrameTimestamp, strerror(AVUNERROR(tDecoderResult)), AVUNERROR(tDecoderResult), tFrameFinished);
                            }
                        }
                        break;

                    case MEDIA_AUDIO:
                        {
                            // assign the only existing plane because the output will be packed audio data
                            uint8_t *tDecodedAudioSamples = tChunkBuffer;

                            // ############################
                            // ### DECODE FRAME
                            // ############################
                            // log statistics
                            if (mDecoderThreadAcountsPackets)
                                AnnouncePacket(tPacket->size);

                            int tOutputAudioBytesPerSample = av_get_bytes_per_sample(mOutputAudioFormat);
                            int tInputAudioBytesPerSample = av_get_bytes_per_sample(mInputAudioFormat);

                            // Decode the next chunk of data
                            tDecoderResult = avcodec_decode_audio4(mCodecContext, tAudioFrame, &tFrameFinished, tPacket);
                            if (tDecoderResult >= 0)
                            {
                                if (tFrameFinished != 0)
                                {
                                    if ((!tAlreadyWarnedThatFrameSizeDiffers) && (tAudioFrame->nb_samples != mCodecContext->frame_size))
                                    {
                                        tAlreadyWarnedThatFrameSizeDiffers = true;
                                        LOG(LOG_WARN, "Audio frame size %d differs from the codec specific frame size %d, will ignore this in the future and prevent further warnings about this", tAudioFrame->nb_samples, mCodecContext->frame_size);
                                    }

                                    // ############################
                                    // ### ANNOUNCE FRAME (statistics)
                                    // ############################
                                    AnnounceFrame(tAudioFrame);

                                    // ############################
                                    // ### RECORD FRAME
                                    // ############################
                                    // re-encode the frame and write it to file
                                    if (mRecording)
                                        RecordFrame(tAudioFrame);

                                    #ifdef MSMEM_DEBUG_AUDIO_FRAME_RECEIVER
                                        LOG(LOG_VERBOSE, "New audio frame..");
                                        LOG(LOG_VERBOSE, "      ..pts: %"PRId64", original PTS: %.2f, BE PTS: %lld", tAudioFrame->pts, (float)tCurrentInputFrameTimestamp, av_frame_get_best_effort_timestamp(tAudioFrame));
                                        LOG(LOG_VERBOSE, "      ..size: %d bytes (%d samples of format %s)", tAudioFrame->nb_samples * tOutputAudioBytesPerSample * mOutputAudioChannels, tAudioFrame->nb_samples, av_get_sample_fmt_name(mOutputAudioFormat));
                                    #endif

                                    if (mAudioResampleContext != NULL)
                                    {// audio resampling needed: we have to insert an intermediate step, which resamples the audio chunk

                                        // ############################
                                        // ### RESAMPLE FRAME (CONVERT)
                                        // ############################
                                        const uint8_t **tAudioFramePlanes = (const uint8_t **)tAudioFrame->extended_data;
                                        int tResampledBytes = (tOutputAudioBytesPerSample * mOutputAudioChannels) * HM_swr_convert(mAudioResampleContext, (uint8_t**)&tDecodedAudioSamples /* only one plane because this is packed audio data */, AVCODEC_MAX_AUDIO_FRAME_SIZE / (tOutputAudioBytesPerSample * mOutputAudioChannels) /* amount of possible output samples */, (const uint8_t**)tAudioFramePlanes, tAudioFrame->nb_samples);
                                        if(tResampledBytes > 0)
                                        {
                                            tCurrentChunkSize = tResampledBytes;
                                        }else
                                        {
                                            LOG(LOG_ERROR, "Amount of resampled bytes (%d) is invalid", tResampledBytes);
                                        }
                                    }else
                                    {// no audio resampling needed
                                        // we can use the input buffer without modifications
                                        tCurrentChunkSize = av_samples_get_buffer_size(NULL, mCodecContext->channels, tAudioFrame->nb_samples, (enum AVSampleFormat) tAudioFrame->format, 1);
                                        tDecodedAudioSamples = tAudioFrame->data[0];
                                    }

                                    if (tCurrentChunkSize > 0)
                                    {
                                        // get the buffered samples (= pts)
                                        int tAudioFifoBufferedSamples = av_fifo_size(mResampleFifo[0]) /* buffer sie in bytes */ / (tOutputAudioBytesPerSample * mOutputAudioChannels);

                                        // ############################
                                        // ### WRITE FRAME TO FIFO
                                        // ############################
                                        #ifdef MSMEM_DEBUG_AUDIO_FRAME_RECEIVER
                                            LOG(LOG_VERBOSE, "Adding %d bytes to AUDIO FIFO with buffer of %d bytes (%d samples), packet pts: %.2f", tCurrentChunkSize, av_fifo_size(mResampleFifo[0]), tAudioFifoBufferedSamples, (float)tCurrentInputFrameTimestamp);
                                        #endif
                                        // is there enough space in the FIFO?
                                        if (av_fifo_space(mResampleFifo[0]) < tCurrentChunkSize)
                                        {// no, we need reallocation
                                            if (av_fifo_realloc2(mResampleFifo[0], av_fifo_size(mResampleFifo[0]) + tCurrentChunkSize - av_fifo_space(mResampleFifo[0])) < 0)
                                            {
                                                // acknowledge failed
                                                LOG(LOG_ERROR, "Reallocation of FIFO audio buffer failed");
                                            }
                                        }

                                        // write new samples into fifo buffer
                                        av_fifo_generic_write(mResampleFifo[0], tDecodedAudioSamples, tCurrentChunkSize, NULL);

                                        // ############################
                                        // ### calculate PTS value
                                        // ############################
                                        // save PTS value to deliver it later to the frame grabbing thread
                                        if ((tAudioFrame->pkt_dts != (int64_t)AV_NOPTS_VALUE) || (tAudioFrame->pkt_pts != (int64_t)AV_NOPTS_VALUE))
                                        {// use PTS/DTS
                                            tCurrentOutputFrameTimestamp = HM_av_frame_get_best_effort_timestamp(tAudioFrame);
                                            #ifdef MSMEM_DEBUG_TIMING
                                                LOG(LOG_VERBOSE, "Setting current frame PTS to frame packet BE PTS: %"PRId64, tCurrentOutputFrameTimestamp);
                                            #endif
                                        }else
                                        {// fall back to packet's PTS value
                                            #ifdef MSMEM_DEBUG_TIMING
                                                LOG(LOG_VERBOSE, "Setting current frame PTS to packet PTS: %.2f", (float)tCurrentInputFrameTimestamp);
                                            #endif
                                            tCurrentOutputFrameTimestamp = tCurrentInputFrameTimestamp;
                                        }

                                        tCurrentOutputFrameNumber = CalculateOutputFrameNumber(tCurrentOutputFrameTimestamp) - (double)tAudioFifoBufferedSamples / MEDIA_SOURCE_SAMPLES_PER_BUFFER /* frame shift */;
                                        //TODO: LOG(LOG_WARN, "%s: %.2f => %.2f", GetMediaTypeStr().c_str(), (float)tCurrentInputFrameTimestamp, (float)tCurrentOutputFrameNumber);

                                        // save PTS value to deliver it later to the frame grabbing thread
                                        #ifdef MSMEM_DEBUG_AUDIO_FRAME_RECEIVER
                                            LOG(LOG_VERBOSE, "Setting current frame nr. to %.2lf, packet PTS: %.2f", tCurrentOutputFrameNumber, (float)tCurrentInputFrameTimestamp);
                                        #endif

                                        // the amount of bytes we want to have in an output frame
                                        int tDesiredOutputSize = MEDIA_SOURCE_SAMPLES_PER_BUFFER * tOutputAudioBytesPerSample * mOutputAudioChannels;

                                        int tLoops = 0;
                                        while (av_fifo_size(mResampleFifo[0]) >= MEDIA_SOURCE_SAMPLES_PER_BUFFER * tOutputAudioBytesPerSample * mOutputAudioChannels)
                                        {
                                            tLoops++;
                                            // ############################
                                            // ### READ FRAME FROM FIFO
                                            // ############################
                                            #ifdef MSMEM_DEBUG_AUDIO_FRAME_RECEIVER
                                                LOG(LOG_VERBOSE, "Loop %d-Reading %d bytes from %d bytes of fifo, current frame nr.: %.2lf", tLoops, tDesiredOutputSize, av_fifo_size(mResampleFifo[0]), tCurrentOutputFrameNumber);
                                            #endif
                                            // read sample data from the fifo buffer
                                            HM_av_fifo_generic_read(mResampleFifo[0], (void*)tChunkBuffer, tDesiredOutputSize);
                                            tCurrentChunkSize = tDesiredOutputSize;

                                            // ############################
                                            // ### WRITE FRAME TO OUTPUT FIFO
                                            // ############################
                                            // add new chunk to FIFO
                                            if (tCurrentChunkSize <= mDecoderFifo->GetEntrySize())
                                            {
                                                #ifdef MSMEM_DEBUG_AUDIO_FRAME_RECEIVER
                                                    LOG(LOG_VERBOSE, "Writing %d %s bytes at %p to output FIFO with frame nr. %.2lf, remaining audio data: %d bytes", tCurrentChunkSize, GetMediaTypeStr().c_str(), tChunkBuffer, tCurrentOutputFrameNumber, av_fifo_size(mResampleFifo[0]));
                                                #endif
                                                WriteOutputChunk((char*)tChunkBuffer, tCurrentChunkSize, (int64_t)rint(tCurrentOutputFrameNumber));

                                                // prepare frame number for next loop
                                                tCurrentOutputFrameNumber += 1;

                                                #ifdef MSMEM_DEBUG_DECODER_STATE
                                                    LOG(LOG_VERBOSE, "Successful audio buffer loop");
                                                #endif
                                            }else
                                            {
                                                LOG(LOG_ERROR, "Cannot write a %s chunk of %d bytes to the FIFO with %d bytes slots", GetMediaTypeStr().c_str(),  tCurrentChunkSize, mDecoderFifo->GetEntrySize());
                                            }
                                        }
                                    }
                                }else
                                {// audio frame was buffered by ffmpeg
                                    LOG(LOG_VERBOSE, "Audio frame was buffered");
                                    if (tPacket->data != NULL)
                                        mDecoderOutputFrameDelay++;
                                }
                            }else
                            {// tDecoderResult < 0
                                LOG(LOG_WARN, "Couldn't decode audio samples %.2f because \"%s\"(%d)", (float)tCurrentInputFrameTimestamp, strerror(AVUNERROR(tDecoderResult)), AVUNERROR(tDecoderResult));
                            }
                        }
                        break;
                    default:
                            LOG(LOG_ERROR, "Media type unknown");
                            break;
                }
                mDecoderResetBuffersMutex.unlock();                
            }

            // free packet buffer
            if (tPacket->data != NULL)
                av_free_packet(tPacket);

            mDecoderNeedWorkConditionMutex.lock();
            if (mEOFReached)
            {// EOF, wait until restart
                // add empty chunk to FIFO
                #ifdef MSMEM_DEBUG_PACKETS
                    LOG(LOG_VERBOSE, "EOF reached, writing empty chunk to %s stream decoder FIFO", GetMediaTypeStr().c_str());
                #endif

                // time to sleep
                #ifdef MSMEM_DEBUG_DECODER_STATE
                    LOG(LOG_VERBOSE, "EOF for %s source reached, wait some time and check again, loop %d", GetMediaTypeStr().c_str(), ++tWaitLoop);
                #endif

                // make sure that the grabber isn't infinitely blocked
                WriteOutputChunk(NULL, 0, 0);

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
                    LOG(LOG_WARN, "Nothing to do for %s decoder, FIFO has %d of %d entries, wait some time and check again, loop %d", GetMediaTypeStr().c_str(), mDecoderFifo->GetUsage(), mDecoderFifo->GetSize(), ++tWaitLoop);
                else
                {
                    LOG(LOG_VERBOSE, "Nothing to do for %s decoder, wait some time and check again, loop %d", GetMediaTypeStr().c_str(), ++tWaitLoop);
                }
            #endif
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
                    CloseVideoScaler(tVideoScaler);
                    LOG(LOG_VERBOSE, "..VIDEO decoder thread stopped scaler thread");
                }else
                {
                    // Free the RGB frame
                    LOG(LOG_VERBOSE, "..releasing RGB frame buffer");
                    av_free(tVideoPictureFrame);
                }

                // Free the YUV frame
                LOG(LOG_VERBOSE, "..releasing SOURCE frame buffer");
                av_free(tVideoSourceFrame);

                break;
        case MEDIA_AUDIO:
                LOG(LOG_VERBOSE, "..releasing AUDIO frame buffer");
                av_free(tAudioFrame);

                break;
        default:
                break;
    }

    LOG(LOG_VERBOSE, "..closing format converter");
    CloseFormatConverter();

    LOG(LOG_VERBOSE, "..releasing chunk buffer");
    av_free(tChunkBuffer);

    LOG(LOG_VERBOSE, "..releasing FIFO buffer");
    delete mDecoderFifo;
    mDecoderFifo = NULL;

    LOG(LOG_WARN, "%s decoder main loop finished for %s media source <<<<<<<<<<<<<<<<", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());

    return NULL;
}

void MediaSourceMem::ResetPreCalculatedData()
{
    mDecoderResetBuffersMutex.lock();

    // reset the library internal frame FIFO
    LOG(LOG_VERBOSE, "Reseting %s decoder internal FIFO after UnregisterMediaFilter()", GetMediaTypeStr().c_str());
    mDecoderFifo->ClearFifo();

    mDecoderResetBuffersMutex.unlock();

    mDecoderNeedWorkCondition.Signal();
}

void MediaSourceMem::ResetDecoderBuffers()
{
    mDecoderResetBuffersMutex.lock();

    // flush ffmpeg internal buffers
    LOG(LOG_VERBOSE, "Reseting %s decoder internal buffers after seeking in input stream", GetMediaTypeStr().c_str());
    avcodec_flush_buffers(mCodecContext);

    // reset the library internal frame FIFO
    LOG(LOG_VERBOSE, "Reseting %s decoder internal FIFO after seeking in input stream", GetMediaTypeStr().c_str());
    mDecoderFifo->ClearFifo();

    if ((mMediaType == MEDIA_AUDIO) && (mResampleFifo[0] != NULL) && (av_fifo_size(mResampleFifo[0]) > 0))
    {
        LOG(LOG_VERBOSE, "Reseting %s decoder internal buffers resample FIFO after seeking in input stream", GetMediaTypeStr().c_str());
        av_fifo_drain(mResampleFifo[0], av_fifo_size(mResampleFifo[0]));
    }

    mDecoderOutputFrameDelay = 0;

    // wait for next key frame before we do anything
    LOG(LOG_VERBOSE, "Waiting for first %s key frame after reset of decoder buffers", GetMediaTypeStr().c_str());
    mDecoderWaitForNextKeyFramePackets = true;
    mDecoderWaitForNextKeyFrame = true;
    mDecoderWaitForNextKeyFrameTimeout = av_gettime() + MSM_WAITING_FOR_FIRST_KEY_FRAME_TIMEOUT * 1000 * 1000;

    mDecoderResetBuffersMutex.unlock();
}

void MediaSourceMem::WriteOutputChunk(char* pChunkBuffer, int pChunkBufferSize, int64_t pChunkNumber)
{
    #ifdef MSMEM_DEBUG_WAITING_TIMING
        // calculate passed time since last call
        int64_t tTimeToLastcall = 0;
        int64_t tTime = Time::GetTimeStamp();
        if (mTimeLastWrittenOutputChunk == 0)
        {// first call
            mTimeLastWrittenOutputChunk = tTime;
        }else
        {// 1+ call
            tTimeToLastcall = tTime - mTimeLastWrittenOutputChunk;
            mTimeLastWrittenOutputChunk = tTime;
        }
        LOG(LOG_VERBOSE, "Time since last call of %s WriteFrameOutputBuffer(): %"PRId64" ms", GetMediaTypeStr().c_str(), tTimeToLastcall / 1000);
    #endif

    if(!mMediaSourceOpened)
    {
        LOG(LOG_WARN, "%s %s decoder was already closed", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());
        return;
    }

    if(!mDecoderThreadNeeded)
    {
        LOG(LOG_WARN, "%s %s decoder thread was already stopped", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());
        return;
    }

    if (mDecoderFifo == NULL)
    {
        LOG(LOG_ERROR, "Invalid %s %s decoder FIFO", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str());
        return;
    }

    #ifdef MSMEM_DEBUG_FRAME_QUEUE
        LOG(LOG_VERBOSE, ">>> Writing %s frame of %d bytes and pts %"PRId64", FIFOs: %d", GetMediaTypeStr().c_str(), pChunkBufferSize, pChunkNumber, mDecoderFifo->GetUsage());
    #endif

    if (pChunkNumber != 0)
        mLastBufferedOutputFrameIndex = pChunkNumber;

    // write A/V data to output FIFO
    mDecoderFifo->WriteFifo(pChunkBuffer, pChunkBufferSize, pChunkNumber);

    // update pre-buffer time value
    UpdateBufferTime();
}

void MediaSourceMem::ReadOutputChunk(char *pChunkBuffer, int &pChunkBufferSize, int64_t &pChunkNumber)
{
    if (mDecoderFifo == NULL)
    {
        return;
        LOG(LOG_ERROR, "Invalid %s decoder FIFO", GetMediaTypeStr().c_str());
    }

    // read A/V data from output FIFO
    mDecoderFifo->ReadFifo(pChunkBuffer, pChunkBufferSize, pChunkNumber);

    #ifdef MSMEM_DEBUG_FRAME_QUEUE
        LOG(LOG_VERBOSE, "Returning from decoder FIFO the %s frame (PTS = %"PRId64"), remaining frames in FIFO: %d", GetMediaTypeStr().c_str(), pChunkNumber, mDecoderFifo->GetUsage());
    #endif

    // update pre-buffer time value
    UpdateBufferTime();
}

double MediaSourceMem::CalculateInputFrameNumber(double pFrameNumber)
{
    double tTransformationFactor = GetInputFrameRate() / GetOutputFrameRate();

    return pFrameNumber * tTransformationFactor;
}

double MediaSourceMem::CalculateOutputFrameNumber(double pFrameNumber)
{
    double tTransformationFactor = GetOutputFrameRate() / GetInputFrameRate();

    return pFrameNumber * tTransformationFactor;
}

void MediaSourceMem::CalculateExpectedOutputPerInputFrame()
{
    switch(mMediaType)
    {
        case MEDIA_AUDIO:
                if (mCodecContext != NULL)
                {
                    int tInputSamplesPerFrame = mCodecContext->frame_size;
                    if (tInputSamplesPerFrame == 0)
                        tInputSamplesPerFrame = MEDIA_SOURCE_SAMPLES_PER_BUFFER; // fall back to the default value

                    int64_t tInputFrameSize = tInputSamplesPerFrame * av_get_bytes_per_sample(mInputAudioFormat) * mInputAudioChannels;

                    float tSizeRatio = ((float)mOutputAudioChannels / mInputAudioChannels) *
                                       ((float)av_get_bytes_per_sample(mOutputAudioFormat) / av_get_bytes_per_sample(mInputAudioFormat)) *
                                       ((float)mOutputAudioSampleRate / mInputAudioSampleRate);

                    int64_t tMaxOutputSize = (int64_t)((float)tInputFrameSize * tSizeRatio);

                    int tBytesPerOutputSample = av_get_bytes_per_sample(mOutputAudioFormat) * mOutputAudioChannels;

                    // round up to next valid size depending on the amount of bytes per output sample
                    tMaxOutputSize = tMaxOutputSize / (tBytesPerOutputSample) * tBytesPerOutputSample + tBytesPerOutputSample;

                    int64_t tOutputFrameSize = MEDIA_SOURCE_SAMPLES_PER_BUFFER * av_get_bytes_per_sample(mOutputAudioFormat) * mOutputAudioChannels;

                    float tMaxPossibleOutputFrames = (float)tMaxOutputSize / tOutputFrameSize;

                    mDecoderExpectedMaxOutputPerInputFrame = rint(tMaxPossibleOutputFrames);

                    //LOG(LOG_ERROR, "Expected max. output frames: %d(%.2f), size ratio: %.2f (%d/%d + %d/%d + %d/%d), max. output size: %"PRId64, tOutputOfNextInputFrame, tMaxPossibleOutputFrames, tSizeRatio, mOutputAudioChannels, mInputAudioChannels, av_get_bytes_per_sample(mOutputAudioFormat), av_get_bytes_per_sample(mInputAudioFormat), mOutputAudioSampleRate, mInputAudioSampleRate, tMaxOutputSize);
                }else
                    mDecoderExpectedMaxOutputPerInputFrame = 8; // a fallback to a "good" value
                break;
        case MEDIA_VIDEO:
                mDecoderExpectedMaxOutputPerInputFrame = 1;
                break;
        default:
                mDecoderExpectedMaxOutputPerInputFrame = 1;
                break;
    }
}

bool MediaSourceMem::DecoderFifoFull()
{
    bool tResult;

    tResult = ((mDecoderFifo == NULL)/* decoder FIFO is invalid */ ||
               (mDecoderFifo->GetUsage() + mDecoderExpectedMaxOutputPerInputFrame > mDecoderFifo->GetSize() - 1 /* check the usage, one slot for a 0 byte signaling chunk*/));

    return tResult;
}

void MediaSourceMem::UpdateBufferTime()
{
    //LOG(LOG_VERBOSE, "Updating pre-buffer time value");

    float tBufferSize = 0;
    if (mDecoderFifo != NULL)
        tBufferSize = mDecoderFifo->GetUsage();

    mDecoderFrameBufferTime = tBufferSize / GetOutputFrameRate();
}

void MediaSourceMem::CalibrateRTGrabbing()
{
    // adopt the stored pts value which represent the start of the media presentation in real-time useconds
    float  tRelativeFrameIndex = (mLastBufferedOutputFrameIndex - CalculateOutputFrameNumber(mInputStartPts)) / mFrameDuration;
    double tRelativeTime = (int64_t)((double)AV_TIME_BASE * tRelativeFrameIndex / GetOutputFrameRate());
    #ifdef MSMEM_DEBUG_WAITING_TIMING
        LOG(LOG_WARN, "Calibrating %s RT playback, old PTS start: %.2f, pre-buffer time: %.2f", GetMediaTypeStr().c_str(), mSourceStartTimeForRTGrabbing, mDecoderFramePreBufferTime);
    #endif
    if (tRelativeTime < 0)
    {
        LOG(LOG_ERROR, "Found invalid relative PTS value of: %.2lf for frame index: %.2f", tRelativeTime, tRelativeFrameIndex);
        tRelativeTime = 0;
    }
    mSourceStartTimeForRTGrabbing = av_gettime() - tRelativeTime  + mSourceTimeShiftForRTGrabbing + mDecoderFramePreBufferTime * AV_TIME_BASE;
    #ifdef MSMEM_DEBUG_CALIBRATION
        LOG(LOG_WARN, "Calibrating %s RT playback: new PTS start: %.2f, rel. frame index: %.2f, rel. time: %.2f ms", GetMediaTypeStr().c_str(), mSourceStartTimeForRTGrabbing, tRelativeFrameIndex, (float)(tRelativeTime / 1000));
    #endif
}

bool MediaSourceMem::WaitForRTGrabbing()
{
    #ifdef MSMEM_DEBUG_WAITING_TIMING
        LOG(LOG_VERBOSE, "###### WaitForRTGrabbing() for frame %lf", mCurrentOutputFrameIndex);
    #endif

    if (mGrabbingStopped)
    {
        LOG(LOG_WARN, "%s-grabbing was stopped, returning immediately", GetMediaTypeStr().c_str());
        return true;
    }

    // return immediately if PTS from grabber is invalid
    if ((mRtpActivated) && (mCurrentOutputFrameIndex < 0))
    {
        LOG(LOG_WARN, "PTS from %s grabber is invalid: %.2lf", GetMediaTypeStr().c_str(), mCurrentOutputFrameIndex);
        return true;
    }

    if((!mRtpActivated /* no time reference based on RTP available? */) && (mDecoderFramePreBufferTime == 0.0 /* no pre-buffering? */))
    {// no time base found

        // don't wait wait and play this frame immediately
        return true;
    }

    // calculate the current (normalized) frame index of the grabber
    float tNormalizedFrameIndexFromGrabber = (mCurrentOutputFrameIndex - CalculateOutputFrameNumber(mInputStartPts)) / mFrameDuration; // the normalized frame index
    // return immediately if RT-grabbing is not possible
    if (tNormalizedFrameIndexFromGrabber < 0)
    {
        LOG(LOG_WARN, "Normalized %s frame index %.2f is invalid, current output frame index: %.2lf, output start PTS: %.2lf, output fps: %.2f, input fps: %.2f, input start PTS: %.2lf, frame duration: %d", GetMediaTypeStr().c_str(), tNormalizedFrameIndexFromGrabber, mCurrentOutputFrameIndex, CalculateOutputFrameNumber(mInputStartPts), GetOutputFrameRate(), GetInputFrameRate(), mInputStartPts, mFrameDuration);
        return true;
    }else{
        //LOG(LOG_VERBOSE, "Normalized %s frame index %.2f is okay, current output frame index: %.2lf, output start PTS: %.2lf, output fps: %.2f, input fps: %.2f, input start PTS: %.2lf, frame duration: %d", GetMediaTypeStr().c_str(), tNormalizedFrameIndexFromGrabber, mCurrentOutputFrameIndex, CalculateOutputFrameNumber(mInputStartPts), GetOutputFrameRate(), GetInputFrameRate(), mInputStartPts, mFrameDuration);
    }

    #ifdef MSMEM_DEBUG_WAITING_TIMING
        // calculate passed time since last call
        int64_t tTimeToLastcall = 0;
        int64_t tTime = Time::GetTimeStamp();
        if (mLastTimeWaitForRTGrabbing == 0)
        {// first call
            mLastTimeWaitForRTGrabbing = tTime;
        }else
        {// 1+ call
            tTimeToLastcall = tTime - mLastTimeWaitForRTGrabbing;
            mLastTimeWaitForRTGrabbing = tTime;
        }
        LOG(LOG_VERBOSE, "Time since last call of %s WaitForRTGrabbing(): %"PRId64" ms", GetMediaTypeStr().c_str(), tTimeToLastcall / 1000);
    #endif

    // the PTS value of the last output frame
    uint64_t tCurrentPtsFromGrabber = (uint64_t)(1000 * tNormalizedFrameIndexFromGrabber / GetOutputFrameRate()); // in ms

    // calculate the PTS offset between the RTCP PTS reference and the last grabbed frame
    int64_t tDesiredPlayOutTime = 1000 * ((int64_t)tCurrentPtsFromGrabber); // in us

    // calculate the current (normalized) play-out time of the current A/V stream
    int64_t tCurrentPlayOutTime = av_gettime() - (int64_t)mSourceStartTimeForRTGrabbing; // in us

    // check if we have already reached the pre-buffer threshold time
    if (tCurrentPlayOutTime < 0)
    {// no pre-buffering is currently running, we should wait until pre-buffer time is reached
        // transform into waiting time
        tCurrentPlayOutTime *= (-1);

        #ifdef MSMEM_DEBUG_WAITING_TIMING
            LOG(LOG_WARN, "%s-%s-sleeping for %"PRId64" ms to reach pre-buffer time", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), tCurrentPlayOutTime / 1000);
        #endif

        // wait until pre-buffer time is reached
        if (tCurrentPlayOutTime <= MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME * AV_TIME_BASE)
            Thread::Suspend(tCurrentPlayOutTime);
        else
        {
            LOG(LOG_WARN, "Found in %s %s source an invalid delay time of %"PRId64" s for reaching pre-buffer threshold time, pre-buffer time: %.2f, PTS of last queued frame: %.2lf, PTS of last grabbed frame: %.2lf", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), tCurrentPlayOutTime / 1000, mDecoderFramePreBufferTime, mDecoderLastReadPts, mCurrentOutputFrameIndex);
            LOG(LOG_WARN, "WaitForRTGrabbing()-Triggering RT-Grabbing calibration");
            mDecoderRecalibrateRTGrabbingAfterSeeking = true;
        }

        // update play-out time
        tCurrentPlayOutTime = av_gettime() - (int64_t)mSourceStartTimeForRTGrabbing; // in us
    }

    // calculate the time offset between the desired and current play-out time, which can be used for a wait cycle (Thread::Suspend)
    int64_t tResultingTimeOffset = tDesiredPlayOutTime - tCurrentPlayOutTime; // in us

    #ifdef MSMEM_DEBUG_WAITING_TIMING
        LOG(LOG_VERBOSE, "%s-current relative frame index: %f, relative time: %"PRIu64" ms (Fps: %3.2f), stream start time: %f us, time difference: %lld us", GetMediaTypeStr().c_str(), tNormalizedFrameIndexFromGrabber, tCurrentPtsFromGrabber, GetInputFrameRate(), (float)mInputStartPts, tResultingTimeOffset);
        LOG(LOG_WARN, "%s-%s-sleeping for %"PRId64" ms (%"PRId64" - %"PRId64") for frame %.2lf, RT ref. time: %.2lf", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), tResultingTimeOffset / 1000, tDesiredPlayOutTime, tCurrentPlayOutTime, mCurrentOutputFrameIndex, mSourceStartTimeForRTGrabbing);
    #endif

    // adapt timing to real-time
    if (tResultingTimeOffset > 0)
    {// waiting time is okay, we have to do active waiting
        if (tResultingTimeOffset <= MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME * AV_TIME_BASE)
        {
            Thread::Suspend(tResultingTimeOffset);
        }else
        {
            LOG(LOG_WARN, "Found in %s %s source an invalid delay time of %"PRId64" s, pre-buffer time: %.2f, PTS of last queued frame: %.2lf, PTS of last grabbed frame: %.2lf", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), tResultingTimeOffset / 1000, mDecoderFramePreBufferTime, mDecoderLastReadPts, mCurrentOutputFrameIndex);
            LOG(LOG_WARN, "WaitForRTGrabbing()-Triggering RT-Grabbing calibration");
            mDecoderRecalibrateRTGrabbingAfterSeeking = true;
            return false;
        }
    }else
    {// waiting time invalid, frame is too late
        float tDelay = (float)tResultingTimeOffset / (-1000);

        if (mRtpActivated)
        {
            if (tDelay > MEDIA_SOURCE_MEM_DEFAULT_E2E_DELAY_JITER * 1000)
            {
                // should we met the pre-buffer time?
                if (mDecoderFramePreBufferTime > 0)
                {// we have to be faster, signal to drop this frame
                    #ifdef MSMEM_DEBUG_WAITING_TIMING
                        LOG(LOG_WARN, "RTP-stream is too late, %s %s grabbing is %f ms too late, THRESHOLD: %lld ms", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), tDelay, (int64_t)(MEDIA_SOURCE_MEM_DEFAULT_E2E_DELAY_JITER * 1000));
                    #endif
                    return false;
                }else
                {// we are late, but there is not other choice
                    // play the delayed frame anyhow
                }
            }else
            {
                //#ifdef MSMEM_DEBUG_WAITING_TIMING
                    LOG(LOG_WARN, "%s %s grabbing is %f ms too late, THRESHOLD: %lld ms", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), tDelay, (int64_t)(MEDIA_SOURCE_MEM_DEFAULT_E2E_DELAY_JITER * 1000));
                //#endif
            }
        }else
        {
            // check if we are still in play-range
            if (tDelay >= MEDIA_SOURCE_MEM_FRAME_DROP_THRESHOLD * 1000)
            {
                #ifdef MSMEM_DEBUG_WAITING_TIMING
                    LOG(LOG_WARN, "System is too slow?, %s %s grabbing is %f ms too late, THRESHOLD: %lld ms", GetMediaTypeStr().c_str(), GetSourceTypeStr().c_str(), tDelay, (int64_t)(MEDIA_SOURCE_MEM_FRAME_DROP_THRESHOLD * 1000));
                #endif
                return false;
            }
        }
    }

    return true;
}

int MediaSourceMem::GetSynchronizationPoints()
{
    return mRtcpSenderReportsReceived;
}

int64_t MediaSourceMem::GetSynchronizationTimestamp()
{
    int64_t tResult = 0;

    if ((mDecoderFifo != NULL) && (mDecoderFifo->GetUsage()) && (mCurrentOutputFrameIndex > 0))
    {// we have some first passed A/V frames, the decoder does not need to re-calibrate the RT grabber
        /******************************************
         * The following lines do the following:
         *   - use the RTCP sender reports and extract the reference RTP timestamps and NTP time
         *     =>> we know when exactly (NTP time!) a given RTP play-out time index passed the processing chain at sender side
         *   - shift the given NTP time from sender side according to the time difference of the local playback in order to conclude the NTP time (at sender side!) for the current output frame
         *     =>> we know exactly when the current frame passed the processing chain at sender side
         *   - return the calculated NTP time as synchronization timestamp in micro seconds
         *
         *   - when this approach is applied for both the video and audio stream (which is received from the same physical host with the same real-time clock inside!),
         *     a time difference can be derived, which corresponds to the A/V drift in micro seconds between the video and audio playback on the local(!) machine
         ******************************************/
        if (mRtpActivated)
        {// RTP active
            // get the reference from RTCP
            uint64_t tReferenceNtpTime = 0;
            uint64_t tReferencePts = 0;
            GetSynchronizationReferenceFromRTP(tReferenceNtpTime, tReferencePts);
            int64_t tReceivedSyncPackets = ReceivedRTCPPackets();
            if (tReferenceNtpTime == 0)
            {// reference values from RTP are still invalid, RTCP packet is needed (expected in some seconds)
                if (tReceivedSyncPackets > 0)
                    LOG(LOG_WARN, "%s NTP time is invalid, received RTCP packets: %"PRId64, GetMediaTypeStr().c_str(), tReceivedSyncPackets);
                else
                {
                    #ifdef MSMEM_DEBUG_AV_SYNC
                        LOG(LOG_WARN, "%s NTP time is invalid, no RTCP packets received yet", GetMediaTypeStr().c_str());
                    #endif
                }
                // nothing to complain about, we return 0 to signal we have no valid synchronization timestamp yet
                return 0;
            }

            tReferencePts = (uint64_t)(1000 * CalculateOutputFrameNumber(tReferencePts) / (((GetMediaType() == MEDIA_AUDIO) && (mSourceCodecId != AV_CODEC_ID_MP3)) ? (float)mOutputAudioSampleRate / 1000 /* audio PTS is the play-out time in samples */: 1  /* video PTS is the play-out time in ms */)); // in ms

            // calculate the current (normalized) frame index of the grabber
            float tNormalizedFrameIndexFromGrabber = mCurrentOutputFrameIndex - CalculateOutputFrameNumber(mInputStartPts); // the normalized frame index

            // the PTS value of the last output frame
            uint64_t tCurrentPtsFromGrabber = (uint64_t)(AV_TIME_BASE * tNormalizedFrameIndexFromGrabber / GetOutputFrameRate()); // in us

            // calculate the PTS offset between the RTCP PTS reference and the last grabbed frame
            int64_t tTimeOffsetToRTPReference = ((int64_t)tCurrentPtsFromGrabber - tReferencePts); // in us

            // calculate the NTP time (we refer to the NTP time of the RTP source) of the currently grabbed frame
            tResult = (int64_t)tReferenceNtpTime + tTimeOffsetToRTPReference;

            #ifdef MSMEM_DEBUG_AV_SYNC
                int64_t tLocalNtpTime = av_gettime();

                //HINT: "diff" value should correlate with the frame buffer time! otherwise something went wrong in the processing chain
                LOG(LOG_VERBOSE, "%s output frame rate: %lf, input frame rate: %lf", GetMediaTypeStr().c_str(), GetOutputFrameRate(), GetInputFrameRate());
                LOG(LOG_VERBOSE, "%s RTP reference: (pts %"PRIu64" / NTP %"PRIu64"), RTP pts playback time: %.2f s", GetMediaTypeStr().c_str(), tReferencePts, tReferenceNtpTime, (float)tReferencePts / 1000);
                LOG(LOG_VERBOSE, "%s normalized local playback time: %lu ms (frame index: %.2f, start PTS: %lf)", GetMediaTypeStr().c_str(), tCurrentPtsFromGrabber / 1000, tNormalizedFrameIndexFromGrabber, CalculateOutputFrameNumber(mInputStartPts));
                LOG(LOG_VERBOSE, "%s time offset to RTP reference: %.2f s (%lu - %lu ms)", GetMediaTypeStr().c_str(), (float)tTimeOffsetToRTPReference / AV_TIME_BASE, tCurrentPtsFromGrabber / 1000, tReferencePts / 1000);
                LOG(LOG_VERBOSE, "%s time diff: %"PRId64" ms (%"PRId64" ms - %"PRId64" ms) ", GetMediaTypeStr().c_str(), (tLocalNtpTime - tResult) / 1000, tLocalNtpTime / 1000, tResult / 1000);
            #endif
            //if (tCurrentPtsFromRTP < tReferencePts)
            //    LOG(LOG_WARN, "Received %s reference (from RTP) PTS value: %u is in the future, last received (RTP) PTS value: %"PRIu64")", GetMediaTypeStr().c_str(), tReferencePts, tCurrentPtsFromRTP);
        }else
        {// no RTP available
            //TODO: we have to implement support for A/V sync. of plain (without RTP encapsulation) A/V streams -> based on relative pre-buffer times (we don't have absolute time references in this case)
        }
    }

    return tResult;
}

bool MediaSourceMem::TimeShift(int64_t pOffset)
{
    if ((pOffset < -MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME * AV_TIME_BASE) || (pOffset > MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME * AV_TIME_BASE))
    {
        LOG(LOG_WARN, "Desired time shift of %d s is out of allowed range (%d) and will be ignored", (int)(pOffset / AV_TIME_BASE), (int)MEDIA_SOURCE_MEM_FRAME_INPUT_QUEUE_MAX_TIME);
        return true; //we signal success because this isn't a real problem
    }

    LOG(LOG_WARN, "Shifting %s time by: %"PRId64, GetMediaTypeStr().c_str(), pOffset);
    mSourceTimeShiftForRTGrabbing -= pOffset;
    LOG(LOG_WARN, "TimeShift()-Triggering RT-Grabbing calibration");
    mDecoderRecalibrateRTGrabbingAfterSeeking = true;
    return true;
}

double MediaSourceMem::CalculateFrameNumberFromRTP()
{
    double tResult = 0;
    float tFrameRate = GetInputFrameRate();
    if (tFrameRate < 1.0)
        return 0;

    if (mFirstReceivedFrameTimestampFromRTP == -1)
    {
        #ifdef MSMEM_DEBUG_PRE_BUFFERING
            LOG(LOG_WARN, "Timestamp from RTP of first received frame is still -1, repairing this..");
        #endif
        mFirstReceivedFrameTimestampFromRTP = (double)GetCurrentPtsFromRTP();
    }

    // the following sequence delivers the frame number independent from packet loss because
    // it calculates the frame number based on the current timestamp from the RTP header
    if ((mMediaType == MEDIA_VIDEO) || (mSourceCodecId == AV_CODEC_ID_MP3 /* for MP3 codec a 90 kHz clock rate is used, similar to video codecs */))
    {
        double tTimeBetweenFrames = 1000 / tFrameRate;
        tResult = mFirstReceivedFrameTimestampFromRTP / tTimeBetweenFrames;
        #ifdef MSMEM_DEBUG_PACKET_TIMING
            LOG(LOG_VERBOSE, "RTP has VIDEO PTS: %.2lf", mFirstReceivedFrameTimestampFromRTP);
        #endif
    }else if (mMediaType == MEDIA_AUDIO)
    {

        //LOG(LOG_VERBOSE, "%.2lf <==> %.2lf, %.2lf", mFirstReceivedFrameTimestampFromRTP, (double)GetCurrentPtsFromRTP(), mFirstReceivedFrameTimestampFromRTP - (double)GetCurrentPtsFromRTP());

        tResult = mFirstReceivedFrameTimestampFromRTP / mCodecContext->frame_size;

        #ifdef MSMEM_DEBUG_PACKET_TIMING
            LOG(LOG_VERBOSE, "RTP has AUDIO PTS: %.2lf", mFirstReceivedFrameTimestampFromRTP);
        #endif
    }else
    {
        LOG(LOG_ERROR, "Reached invalid state");
    }

    #ifdef MSMEM_DEBUG_PRE_BUFFERING
        LOG(LOG_VERBOSE, "Calculated a frame number: %.2f (RTP timestamp: %.2lf), fps: %.2f", (float)tResult, mFirstReceivedFrameTimestampFromRTP, tFrameRate);
    #endif

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
