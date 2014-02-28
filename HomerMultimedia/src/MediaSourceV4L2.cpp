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
 * Purpose: Implementation of a ffmpeg based local v4l2 video source
 * Author:  Thomas Volkert
 * Since:   2008-12-01
 */

#include <MediaSourceV4L2.h>
#include <MediaSource.h>
#include <ProcessStatisticService.h>
#include <Logger.h>
#include <Header_Ffmpeg.h>

#include <cstdio>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

MediaSourceV4L2::MediaSourceV4L2(string pDesiredDevice):
    MediaSource("V4L2: local capture")
{
    mSourceType = SOURCE_DEVICE;
    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    mCurrentInputChannelName = "";

    bool tNewDeviceSelected = false;
    SelectDevice(pDesiredDevice, MEDIA_VIDEO, tNewDeviceSelected);
    if (!tNewDeviceSelected)
    {
        LOG(LOG_INFO, "Haven't selected new v4l2 device when creating source object");
    }
}

MediaSourceV4L2::~MediaSourceV4L2()
{
    if (mMediaSourceOpened)
        CloseGrabDevice();
}

void MediaSourceV4L2::getVideoDevices(VideoDevices &pVList)
{
    static bool tFirstCall = true;
    struct v4l2_capability tV4L2Caps;
    struct v4l2_input tV4L2Input;

    VideoDeviceDescriptor tDevice;
    string tDeviceFile;
    FILE *tFile;
    int tFd;

    #ifdef MSV_DEBUG_PACKETS
        tFirstCall = true;
    #endif

    if (tFirstCall)
        LOG(LOG_VERBOSE, "Enumerating hardware..");

    for (int tDeviceId = 0; tDeviceId != 10; tDeviceId++)
    {
        tDeviceFile = "/dev/video";
        tDeviceFile += char(tDeviceId + 48);
        if (((tFile = fopen(tDeviceFile.c_str(), "r")) != NULL) && (fclose(tFile) != EOF))
        {
            tDevice.Name = "V4L2 device ";
            tDevice.Name += char(tDeviceId + 48);
            tDevice.Card = tDeviceFile;
            tDevice.Desc = "V4L2 based video device";
            tDevice.Type = GeneralVideoDevice;

            if (tFirstCall)
                if (tDevice.Name.size())
                    LOG(LOG_VERBOSE, "Found video device: %s (device file: %s)", tDevice.Name.c_str(), tDeviceFile.c_str());

            if ((tFd = open(tDeviceFile.c_str(), O_RDONLY)) >= 0)
            {
                if (ioctl(tFd, VIDIOC_QUERYCAP, &tV4L2Caps) < 0)
                    LOG(LOG_ERROR, "Can't get device capabilities for \"%s\" because of \"%s\"", tDeviceFile.c_str(), strerror(errno));
                else
                {
                    tDevice.Name = toString(tV4L2Caps.card);
                    tDevice.Desc += " \"" + toString(tV4L2Caps.card) + "\"";
                    if (tFirstCall)
                    {
                        LOG(LOG_VERBOSE, "..driver name: %s", tV4L2Caps.driver);
                        LOG(LOG_VERBOSE, "..card name: %s", tV4L2Caps.card);
                        LOG(LOG_VERBOSE, "..connected at: %s", tV4L2Caps.bus_info);
                        LOG(LOG_VERBOSE, "..driver version: %u.%u.%u", (tV4L2Caps.version >> 16) & 0xFF, (tV4L2Caps.version >> 8) & 0xFF, tV4L2Caps.version & 0xFF);
                        if (tV4L2Caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)
                            LOG(LOG_VERBOSE, "supporting video capture interface");
                        if (tV4L2Caps.capabilities & V4L2_CAP_VIDEO_OUTPUT)
                            LOG(LOG_VERBOSE, "supporting video output interface");
                        if (tV4L2Caps.capabilities & V4L2_CAP_VIDEO_OVERLAY)
                            LOG(LOG_VERBOSE, "supporting video overlay interface");
                        if (tV4L2Caps.capabilities & V4L2_CAP_TUNER)
                            LOG(LOG_VERBOSE, "..onboard tuner");
                        if (tV4L2Caps.capabilities & V4L2_CAP_AUDIO)
                            LOG(LOG_VERBOSE, "..onboard audio");
                        if (tV4L2Caps.capabilities & V4L2_CAP_RDS_CAPTURE)
                            LOG(LOG_VERBOSE, "..onboard RDS");
                        if (tV4L2Caps.capabilities & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)
                            LOG(LOG_VERBOSE, "..supporting OnScreenDisplay (OSD)");
                    }
                }

                int tIndex = 0;
                for(;;)
                {
                    memset(&tV4L2Input, 0, sizeof(tV4L2Input));
                    tV4L2Input.index = tIndex;
                    if (ioctl(tFd, VIDIOC_ENUMINPUT, &tV4L2Input) < 0)
                        break;
                    else
                    {
                        switch(tV4L2Input.type)
                        {
                            case V4L2_INPUT_TYPE_TUNER:
                                    if(tDevice.Type != Camera)
                                        tDevice.Type = Tv;
                                    break;
                            case V4L2_INPUT_TYPE_CAMERA:
                                    if (tDevice.Type != Tv)
                                        tDevice.Type = Camera;
                                    break;
                        }

                        if (tFirstCall)
                        {
                            LOG(LOG_VERBOSE, "..input index: %u", tV4L2Input.index);
                            LOG(LOG_VERBOSE, "..input name: %u", tV4L2Input.name);
                            switch(tV4L2Input.type)
                            {
                                case V4L2_INPUT_TYPE_TUNER:
                                        LOG(LOG_VERBOSE, "..input type: tuner");
                                        break;
                                case V4L2_INPUT_TYPE_CAMERA:
                                        LOG(LOG_VERBOSE, "..input type: camera");
                                        break;
                            }
                            // audioset
                            // tuner
                            // std
                            LOG(LOG_VERBOSE, "..input status: 0x%x", tV4L2Input.status);
                        }
                    }
                    tIndex++;
                }

                close(tFd);
            }

            pVList.push_back(tDevice);
        }
    }
    tFirstCall = false;
}

///////////////////////////////////////////////////////////////////////////////

bool MediaSourceV4L2::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    FILE                *tFile;
    int                 tResult;
    AVDictionary        *tOptions = NULL;
    AVInputFormat       *tFormat;
    AVCodec             *tCodec;
    bool                tSelectedInputSupportsFps = false;

    mMediaType = MEDIA_VIDEO;

    if (pFps > 29.97)
        pFps = 29.97;
    if (pFps < 5)
        pFps = 5;

    LOG(LOG_VERBOSE, "Trying to open the video source");

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(V4L2)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    if (mMediaSourceOpened)
        return false;

    LOG(LOG_VERBOSE, "Desired device is \"%s\"", mDesiredDevice.c_str());

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
        mDesiredDevice = "";

    //########################################################################################################
    // ### check if the selected input from the selected device is a camera device and has no overlay support
    //########################################################################################################
    if (mDesiredDevice != "")
    {
        struct v4l2_capability tV4L2Caps;
        struct v4l2_input tV4L2Input;
        int tFd = 0;
        bool tOverlaySupport = true;

        if ((tFd = open(mDesiredDevice.c_str(), O_RDONLY)) >= 0)
        {
            if (ioctl(tFd, VIDIOC_QUERYCAP, &tV4L2Caps) < 0)
                LOG(LOG_ERROR, "Can't get device capabilities for \"%s\" because of \"%s\"", mDesiredDevice.c_str(), strerror(errno));
            else
            {
                if (!(tV4L2Caps.capabilities & V4L2_CAP_VIDEO_OVERLAY))
                {
                    tOverlaySupport = false;
                }else
                    LOG(LOG_INFO, "Selected input device supports overlay");

                tV4L2Input.index = mDesiredInputChannel;
                if (ioctl(tFd, VIDIOC_ENUMINPUT, &tV4L2Input) < 0)
                {// device has no multi-channel support
                    mCurrentInputChannelName = "";
                }else
                {
                    switch(tV4L2Input.type)
                    {
                        case V4L2_INPUT_TYPE_TUNER:
                                LOG(LOG_INFO, "Selected input %d of device \"%s\" is a tuner", mDesiredInputChannel, mDesiredDevice.c_str());
                                break;
                        case V4L2_INPUT_TYPE_CAMERA:
                                LOG(LOG_INFO, "Selected input %d of device \"%s\" is a camera", mDesiredInputChannel, mDesiredDevice.c_str());
                                if (!tOverlaySupport)
                                    tSelectedInputSupportsFps = true;
                                break;
                    }
                    mCurrentInputChannelName = toString(tV4L2Input.name);
                }
            }
            close(tFd);
        }
    }

    //##################################################################################
    // ### begin to open the selected input from the selected device
    //##################################################################################
    av_dict_set(&tOptions, "channel", toString(mDesiredInputChannel).c_str(), 0);
    av_dict_set(&tOptions, "video_size", (toString(pResX) + "x" + toString(pResY)).c_str(), 0);
    if (tSelectedInputSupportsFps)
        av_dict_set(&tOptions, "framerate", toString((int)pFps).c_str(), 0);

    tFormat = av_find_input_format("video4linux2");
    if (tFormat == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find input format");
        return false;
    }

    // alocate new format context
    mFormatContext = AV_NEW_FORMAT_CONTEXT();

    bool tFound = false;
    if (mDesiredDevice != "")
    {
        //########################################
        //### probing given device file
        //########################################
        tResult = 0;
        if (((tFile = fopen(mDesiredDevice.c_str(), "r")) == NULL) || (fclose(tFile) != 0) || ((tResult = avformat_open_input(&mFormatContext, mDesiredDevice.c_str(), tFormat, &tOptions)) != 0))
        {
            if (tResult != 0)
                LOG(LOG_ERROR, "Couldn't open device \"%s\" because of \"%s\".", mDesiredDevice.c_str(), strerror(AVUNERROR(tResult)));
            else
                LOG(LOG_ERROR, "Couldn't find device \"%s\".", mDesiredDevice.c_str());
            return false;
        }
        //######################################################
        //### retrieve stream information and find right stream
        //######################################################
        if (DetectAllStreams())
            tFound = true;
    }
    if(!tFound)
    {
        //########################################
        //### auto probing possible device files
        //########################################
        LOG(LOG_VERBOSE, "Auto-probing for VFL2 capture device");
        string tDesiredDevice;
        for (int i = 0; i < 10; i++)
        {
            tDesiredDevice = "/dev/video" + toString(i);
            tResult = 0;
            if (((tFile = fopen(tDesiredDevice.c_str(), "r")) != NULL) && (fclose(tFile) == 0) && ((tResult = avformat_open_input(&mFormatContext, tDesiredDevice.c_str(), tFormat, &tOptions)) == 0))
            {
                //######################################################
                //### retrieve stream information and find right stream
                //######################################################
                if (DetectAllStreams())
                {
                    tFound = true;
                    break;
                }else
                    continue;

            }
        }
        if (!tFound)
        {
            LOG(LOG_WARN, "Couldn't find a fitting V4L2 video device");
            return false;
        }
        mDesiredDevice = tDesiredDevice;
    }

    mCurrentDevice = mDesiredDevice;
    mCurrentInputChannel = mDesiredInputChannel;

    VideoDevices tAvailDevs;
    VideoDevices::iterator tDevIt;
    getVideoDevices(tAvailDevs);
    for(tDevIt = tAvailDevs.begin(); tDevIt != tAvailDevs.end(); tDevIt++)
    {
        if(tDevIt->Card == mCurrentDevice)
            mCurrentDeviceName = tDevIt->Name;
    }

    if (!SelectStream())
        return false;

    mFormatContext->streams[mMediaStreamIndex]->time_base.num = 100;
    mFormatContext->streams[mMediaStreamIndex]->time_base.den = (int)pFps * 100;

    if (!OpenDecoder())
        return false;

    if (!OpenFormatConverter())
        return false;

    //###########################################################################################
    //### seek to the current position and drop data received during codec auto detection phase
    //##########################################################################################
    av_seek_frame(mFormatContext, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->cur_dts, AVSEEK_FLAG_ANY);

    // Allocate video frame for source and RGB format
    if ((mSourceFrame = AllocFrame()) == NULL)
        return false;
    if ((mRGBFrame = AllocFrame()) == NULL)
        return false;

    MarkOpenGrabDeviceSuccessful();

    LOG(LOG_INFO, "    ..input: %s", mCurrentInputChannelName.c_str());

    mSupportsMultipleInputChannels = DoSupportsMultipleInputChannels();

    return true;
}

bool MediaSourceV4L2::OpenAudioGrabDevice(int pSampleRate, int pChannels)
{
    LOG(LOG_ERROR, "Wrong media type");
    return false;
}

bool MediaSourceV4L2::CloseGrabDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close");

    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type");
        return false;
    }

    if (mMediaSourceOpened)
    {
        CloseAll();

        // Free the frames
        av_free(mRGBFrame);
        av_free(mSourceFrame);

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mGrabbingStopped = false;
    mSupportsMultipleInputChannels = false;
    mMediaType = MEDIA_UNKNOWN;

    ResetPacketStatistic();

    return tResult;
}

int MediaSourceV4L2::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    AVPacket            tPacket;
    int                 tFrameFinished = 0;
    int                 tBytesDecoded = 0;

    // lock grabbing
    mGrabMutex.lock();

    if (pChunkBuffer == NULL)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("grab buffer is NULL");

        return GRAB_RES_INVALID;
    }

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("video source is closed");

        return GRAB_RES_INVALID;
    }

    if (mGrabbingStopped)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("video source paused");

        return GRAB_RES_INVALID;
    }

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    avpicture_fill((AVPicture *)mRGBFrame, (uint8_t *)pChunkBuffer, PIX_FMT_RGB32, mTargetResX, mTargetResY);

    // Read new packet
    // return 0 if OK, < 0 if error or end of file.
    do
    {
        // read next frame from video source - blocking
        if (av_read_frame(mFormatContext, &tPacket) != 0)
        {
            // unlock grabbing
            mGrabMutex.unlock();

            // acknowledge failed
            MarkGrabChunkFailed("couldn't read next video frame");

            return GRAB_RES_INVALID;
        }
    }while (tPacket.stream_index != mMediaStreamIndex);

    if ((tPacket.data != NULL) && (tPacket.size > 0))
    {
        #ifdef MSV_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Grabbed new video packet:");
            LOG(LOG_VERBOSE, "      ..duration: %d", tPacket.duration);
            LOG(LOG_VERBOSE, "      ..pts: %"PRId64" stream [%d] pts: %"PRId64"", tPacket.pts, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->pts);
            LOG(LOG_VERBOSE, "      ..dts: %"PRId64"", tPacket.dts);
            LOG(LOG_VERBOSE, "      ..size: %d", tPacket.size);
            LOG(LOG_VERBOSE, "      ..pos: %"PRId64"", tPacket.pos);
        #endif

        // log statistics about original packets from device
        AnnouncePacket(tPacket.size);

        // decode packet and get a frame
        if ((!pDropChunk) || (mRecording))
        {
            // Decode the next chunk of data
            tBytesDecoded = HM_avcodec_decode_video(mCodecContext, mSourceFrame, &tFrameFinished, &tPacket);

            // emulate set FPS
            mSourceFrame->pts = GetPtsFromFpsEmulator();

//        // transfer the presentation time value
//        mSourceFrame->pts = tPacket.pts;

            #ifdef MSV_DEBUG_PACKETS
                LOG(LOG_VERBOSE, "Source video frame..");
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
                LOG(LOG_VERBOSE, "      ..pts: %"PRId64"", mSourceFrame->pts);
                LOG(LOG_VERBOSE, "      ..coded pic number: %d", mSourceFrame->coded_picture_number);
                LOG(LOG_VERBOSE, "      ..display pic number: %d", mSourceFrame->display_picture_number);
            #endif

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
                    HM_sws_scale(mVideoScalerContext, mSourceFrame->data, mSourceFrame->linesize, 0, mCodecContext->height, mRGBFrame->data, mRGBFrame->linesize);
                }
            }else
            {
                // unlock grabbing
                mGrabMutex.unlock();

                // acknowledge failed
                MarkGrabChunkFailed("couldn't decode video frame");

                return GRAB_RES_INVALID;
            }
        }

        av_free_packet(&tPacket);
    }

    // return size of decoded frame
    pChunkSize = avpicture_get_size(PIX_FMT_RGB32, mTargetResX, mTargetResY) * sizeof(uint8_t);

    // unlock grabbing
    mGrabMutex.unlock();

    mFrameNumber++;

    // acknowledge success
    MarkGrabChunkSuccessful(mFrameNumber);

    return mFrameNumber;
}

GrabResolutions MediaSourceV4L2::GetSupportedVideoGrabResolutions()
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

        tFormat.Name="VGA";       //       640 x 480
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

        tFormat.Name="SVGA";       //      800 x 600
        tFormat.ResX = 800;
        tFormat.ResY = 600;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="WVGA";       //      854 x 480
        tFormat.ResX = 864;
        tFormat.ResY = 480;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="CIF9";       //     1056 x 864
        tFormat.ResX = 1056;
        tFormat.ResY = 864;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="EDTV";       //     1280 × 720
        tFormat.ResX = 1280;
        tFormat.ResY = 720;
        mSupportedVideoFormats.push_back(tFormat);

        tFormat.Name="HDTV";       //     1920 x 1080
        tFormat.ResX = 1920;
        tFormat.ResY = 1080;
        mSupportedVideoFormats.push_back(tFormat);
    }

    return mSupportedVideoFormats;
}

bool MediaSourceV4L2::HasVariableVideoOutputFrameRate()
{
    return true;
}

bool MediaSourceV4L2::SupportsDecoderFrameStatistics()
{
    return (mMediaType == MEDIA_VIDEO);
}

string MediaSourceV4L2::GetSourceCodecStr()
{
    return "Raw";
}

string MediaSourceV4L2::GetSourceCodecDescription()
{
    return "Raw";
}

bool MediaSourceV4L2::SupportsRecording()
{
    return true;
}

bool MediaSourceV4L2::SupportsMultipleInputStreams()
{
    return mSupportsMultipleInputChannels;
}

bool MediaSourceV4L2::DoSupportsMultipleInputChannels()
{
    int tCount = 0;
    struct v4l2_input tV4L2Input;
    int tFd = 0;
    bool tResult = false;

    //LOG(LOG_VERBOSE, "Probing for multiple input channels for device %s", mCurrentDevice.c_str());
    if ((tFd = open(mCurrentDevice.c_str(), O_RDONLY)) >= 0)
    {
        tV4L2Input.index = 0;
        for(;;)
        {
            if (ioctl(tFd, VIDIOC_ENUMINPUT, &tV4L2Input) < 0)
                break;
            else
                tCount++;
            tV4L2Input.index++;
        }
        close(tFd);
    }

    return tCount > 1;
}

bool MediaSourceV4L2::SelectDevice(std::string pDeviceName, enum MediaType pMediaType, bool &pIsNewDevice)
{
    bool tResult = false;

    tResult = MediaSource::SelectDevice(pDeviceName, pMediaType, pIsNewDevice);

    if (tResult)
        mDesiredInputChannel = 0;

    return tResult;
}

bool MediaSourceV4L2::SelectInputStream(int pIndex)
{
    bool tResult = false;

    if ((pIndex >= 0) && (pIndex < 16))
    {
        if (mCurrentInputChannel != pIndex)
        {
            LOG(LOG_VERBOSE, "Selecting input channel: %d", pIndex);

            if (mCurrentInputChannel != pIndex)
                tResult = true;

            mDesiredInputChannel = pIndex;

            if (tResult)
                Reset();
        }else
        {
            LOG(LOG_VERBOSE, "Selection of input channel skipped because it is already selected");
            tResult = true;
        }
    }else
    {
        LOG(LOG_WARN, "Selected input channel %d is out of range", pIndex);
    }

    return tResult;
}

vector<string> MediaSourceV4L2::GetInputStreams()
{
    vector<string> tResult;

    struct v4l2_input tV4L2Input;
    int tFd = 0;

    // lock grabbing
    mGrabMutex.lock();

    if ((tFd = open(mDesiredDevice.c_str(), O_RDONLY)) >= 0)
    {
        tV4L2Input.index = 0;
        for(;;)
        {
            if (ioctl(tFd, VIDIOC_ENUMINPUT, &tV4L2Input) < 0)
                break;
            else
                tResult.push_back(toString(tV4L2Input.name));
            tV4L2Input.index++;
        }
        close(tFd);
    }

    // unlock grabbing
    mGrabMutex.unlock();

    return tResult;
}

string MediaSourceV4L2::CurrentInputStream()
{
    if (SupportsMultipleInputStreams())
        return mCurrentInputChannelName;
    else
        return mCurrentDevice;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
