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
 * Purpose: Implementation of a ffmpeg based local v4l2 video source
 * Author:  Thomas Volkert
 * Since:   2008-12-01
 */

#include <MediaSourceV4L2.h>
#include <MediaSource.h>
#include <ProcessStatisticService.h>
#include <Logger.h>

#include <cstdio>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <fcntl.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

MediaSourceV4L2::MediaSourceV4L2(string pDesiredDevice):
    MediaSource("V4L2: local capture")
{
    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, PACKET_TYPE_RAW);

    mCurrentInputChannelName = "";
    mCurrentInputChannel = 0;
    mDesiredInputChannel = 0;

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

void MediaSourceV4L2::getVideoDevices(VideoDevicesList &pVList)
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
            tDevice.Name = "V4L2: device ";
            tDevice.Name += char(tDeviceId + 48);
            tDevice.Card = tDeviceFile;
            tDevice.Desc = "V4L2 based video device";

            if (tFirstCall)
                if (tDevice.Name.size())
                    LOG(LOG_VERBOSE, "Found video device: %s (device file: %s)", tDevice.Name.c_str(), tDeviceFile.c_str());

            if ((tFd = open(tDeviceFile.c_str(), O_RDONLY)) >= 0)
            {
                if (ioctl(tFd, VIDIOC_QUERYCAP, &tV4L2Caps) < 0)
                    LOG(LOG_ERROR, "Can't get device capabilities for \"%s\" because of \"%s\"", tDeviceFile.c_str(), strerror(errno));
                else
                {
                    tDevice.Name = "V4L2: " + toString(tV4L2Caps.card);
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

                if (tFirstCall)
                {
                    tV4L2Input.index = 0;
                    for(;;)
                    {
                        if (ioctl(tFd, VIDIOC_ENUMINPUT, &tV4L2Input) < 0)
                            break;
                        else
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
                        tV4L2Input.index++;
                    }
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
    AVFormatParameters  tFormatParams;
    AVInputFormat       *tFormat;
    AVCodec             *tCodec;
    bool                tSelectedInputSupportsFps = false;

    LOG(LOG_VERBOSE, "Trying to open the video source");

    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return false;
    }

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(V4L2)");

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
    tFormatParams.channel = mDesiredInputChannel;
    tFormatParams.standard = NULL;
    if (tSelectedInputSupportsFps)
    {
        tFormatParams.time_base.num = 100;
        tFormatParams.time_base.den = (int)pFps * 100;
    }else
    {
        tFormatParams.time_base.num = 0;
        tFormatParams.time_base.den = 0;
    }
    LOG(LOG_VERBOSE, "Desired time_base: %d/%d (%3.2f)", tFormatParams.time_base.den, tFormatParams.time_base.num, pFps);
    tFormatParams.width = pResX;
    tFormatParams.height = pResY;
    tFormatParams.initial_pause = 0;
    tFormatParams.prealloced_context = 0;
    tFormat = av_find_input_format("video4linux2");
    if (tFormat == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find input format");
        return false;
    }

    if (mDesiredDevice != "")
    {
        //########################################
        //### probing given device file
        //########################################
        tResult = 0;
        if (((tFile = fopen(mDesiredDevice.c_str(), "r")) == NULL) || (fclose(tFile) != 0) || ((tResult = av_open_input_file(&mFormatContext, mDesiredDevice.c_str(), tFormat, 0, &tFormatParams)) != 0))
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
        if ((tResult = av_find_stream_info(mFormatContext)) < 0)
        {
            LOG(LOG_ERROR, "Couldn't find stream information because of \"%s\".", strerror(AVUNERROR(tResult)));
            // Close the V4L2 video file
            av_close_input_file(mFormatContext);
            return false;
        }
    }else
    {
        //########################################
        //### auto probing possible device files
        //########################################
        LOG(LOG_VERBOSE, "Auto-probing for VFL2 capture device");
        bool tFound = false;
        string tDesiredDevice;
        for (int i = 0; i < 10; i++)
        {
            tDesiredDevice = "/dev/video" + toString(i);
            tResult = 0;
            if (((tFile = fopen(tDesiredDevice.c_str(), "r")) != NULL) && (fclose(tFile) == 0) && ((tResult = av_open_input_file(&mFormatContext, tDesiredDevice.c_str(), tFormat, 0, &tFormatParams)) == 0))
            {
                //######################################################
                //### retrieve stream information and find right stream
                //######################################################
                if ((tResult = av_find_stream_info(mFormatContext)) < 0)
                {
                    LOG(LOG_WARN, "Couldn't find stream information because of \"%s\".", strerror(AVUNERROR(tResult)));
                    // Close the V4L2 video file
                    av_close_input_file(mFormatContext);
                    continue;
                }else
                {
                    tFound = true;
                    break;
                }
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

    // Find the first video stream
    mMediaStreamIndex = -1;
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
        // Close the V4L2 video file
        av_close_input_file(mFormatContext);
        return false;
    }

    //######################################################
    //### dump ffmpeg information about format
    //######################################################
    mFormatContext->streams[mMediaStreamIndex]->time_base.num = 100;
    mFormatContext->streams[mMediaStreamIndex]->time_base.den = (int)pFps * 100;

    dump_format(mFormatContext, mMediaStreamIndex, "MediaSourceV4L2 (video)", false);

    // Get a pointer to the codec context for the video stream
    mCodecContext = mFormatContext->streams[mMediaStreamIndex]->codec;

    // set grabbing resolution and frame-rate to the resulting ones delivered by opened video codec
    mSourceResX = mCodecContext->width;
    mSourceResY = mCodecContext->height;
    mFrameRate = (float)mFormatContext->streams[mMediaStreamIndex]->time_base.den / mFormatContext->streams[mMediaStreamIndex]->time_base.num;

    //######################################################
    //### search for correct decoder for the video stream
    //######################################################
    if((tCodec = avcodec_find_decoder(mCodecContext->codec_id)) == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find a fitting codec");
        // Close the V4L2 video file
        av_close_input_file(mFormatContext);
        return false;
    }

    //######################################################
    //### open the selected codec
    //######################################################
    // Inform the codec that we can handle truncated bitstreams -- i.e.,
    // bitstreams where frame boundaries can fall in the middle of packets
    if(tCodec->capabilities & CODEC_CAP_TRUNCATED)
        mCodecContext->flags |= CODEC_FLAG_TRUNCATED;

    if ((tResult = avcodec_open(mCodecContext, tCodec)) < 0)
    {
        LOG(LOG_ERROR, "Couldn't open codec because of \"%s\".", strerror(AVUNERROR(tResult)));
        // Close the V4L2 video file
        av_close_input_file(mFormatContext);
        return false;
    }

    //######################################################
    //### create context for picture scaler
    //######################################################
    mScalerContext = sws_getContext(mCodecContext->width, mCodecContext->height, mCodecContext->pix_fmt, mTargetResX, mTargetResY, PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);

    //###########################################################################################
    //### seek to the current position and drop data received during codec auto detection phase
    //##########################################################################################
    av_seek_frame(mFormatContext, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->cur_dts, AVSEEK_FLAG_ANY);

    mMediaType = MEDIA_VIDEO;
    MarkOpenGrabDeviceSuccessful();
    LOG(LOG_INFO, "    ..input: %s", mCurrentInputChannelName.c_str());

    return true;
}

bool MediaSourceV4L2::OpenAudioGrabDevice(int pSampleRate, bool pStereo)
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
        mMediaSourceOpened = false;

        // free the software scaler context
        sws_freeContext(mScalerContext);

        // Close the V4L2 codec
        avcodec_close(mCodecContext);

        // Close the V4L2 video file
        av_close_input_file(mFormatContext);

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mGrabbingStopped = false;
    mMediaType = MEDIA_UNKNOWN;

    ResetPacketStatistic();

    return tResult;
}

int MediaSourceV4L2::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    AVFrame             *tSourceFrame, *tRGBFrame;
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

        return -1;
    }

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("video source is closed");

        return -1;
    }

    if (mGrabbingStopped)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("video source paused");

        return -1;
    }

    // Allocate video frame structure for source and RGB format
    if (((tSourceFrame = avcodec_alloc_frame()) == NULL) || ((tRGBFrame = avcodec_alloc_frame()) == NULL))
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("out of memory");

        return -1;
    }

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    avpicture_fill((AVPicture *)tRGBFrame, (uint8_t *)pChunkBuffer, PIX_FMT_RGB32, mTargetResX, mTargetResY);

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

            return -1;
        }
    }while (tPacket.stream_index != mMediaStreamIndex);

    if ((tPacket.data != NULL) && (tPacket.size > 0))
    {
        #ifdef MSV_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Grabbed new video packet:");
            LOG(LOG_VERBOSE, "      ..duration: %d", tPacket.duration);
            LOG(LOG_VERBOSE, "      ..pts: %ld stream [%d] pts: %ld", tPacket.pts, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->pts);
            LOG(LOG_VERBOSE, "      ..dts: %ld", tPacket.dts);
            LOG(LOG_VERBOSE, "      ..size: %d", tPacket.size);
            LOG(LOG_VERBOSE, "      ..pos: %ld", tPacket.pos);
        #endif

        // log statistics about original packets from device
        AnnouncePacket(tPacket.size);

        // decode packet and get a frame
        if ((!pDropChunk) || (mRecording))
        {
            // Decode the next chunk of data
            tBytesDecoded = HM_avcodec_decode_video(mCodecContext, tSourceFrame, &tFrameFinished, &tPacket);
        }

        // emulate set FPS
        tSourceFrame->pts = FpsEmulationGetPts();
//        // transfer the presentation time value
//        tSourceFrame->pts = tPacket.pts;

        #ifdef MSV_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Source video frame..");
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

        // re-encode the frame and write it to file
        if (mRecording)
            RecordFrame(tSourceFrame);

        // convert frame to RGB format
        if (!pDropChunk)
        {
            if ((tFrameFinished) && (tBytesDecoded > 0))
                HM_sws_scale(mScalerContext, tSourceFrame->data, tSourceFrame->linesize, 0, mCodecContext->height, tRGBFrame->data, tRGBFrame->linesize);
            else
            {
                // unlock grabbing
                mGrabMutex.unlock();

                // acknowledge failed
                MarkGrabChunkFailed("couldn't decode video frame");

                return -1;
            }
        }

        av_free_packet(&tPacket);
    }

    // return size of decoded frame
    pChunkSize = avpicture_get_size(PIX_FMT_RGB32, mTargetResX, mTargetResY) * sizeof(uint8_t);

    // Free the RGB frame
    av_free(tRGBFrame);

    // Free the YUV frame
    av_free(tSourceFrame);

    // unlock grabbing
    mGrabMutex.unlock();

    // acknowledge success
    MarkGrabChunkSuccessful();

    return ++mChunkNumber;
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

string MediaSourceV4L2::GetCodecName()
{
    return "Raw";
}

string MediaSourceV4L2::GetCodecLongName()
{
    return "Raw";
}

bool MediaSourceV4L2::SupportsMultipleInputChannels()
{
    int tCount = 0;
    struct v4l2_input tV4L2Input;
    int tFd = 0;
    bool tResult = false;

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
                tCount++;
            tV4L2Input.index++;
        }
        close(tFd);
    }

    // unlock grabbing
    mGrabMutex.unlock();

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

bool MediaSourceV4L2::SelectInputChannel(int pIndex)
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Selecting input channel: %d", pIndex);

    if (mCurrentInputChannel != pIndex)
        tResult = true;

    mDesiredInputChannel = pIndex;

    if (tResult)
        Reset();

    return tResult;
}

list<string> MediaSourceV4L2::GetInputChannels()
{
    list<string> tResult;

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

string MediaSourceV4L2::CurrentInputChannel()
{
    if (SupportsMultipleInputChannels())
        return mCurrentInputChannelName;
    else
        return mCurrentDevice;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
