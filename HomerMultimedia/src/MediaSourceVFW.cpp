/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a ffmpeg based local VFW video source
 * Author:  Thomas Volkert
 * Since:   2010-10-19
 */

#include <MediaSourceVFW.h>
#include <MediaSource.h>
#include <ProcessStatisticService.h>
#include <Logger.h>

#include <cstdio>
#include <string.h>
#include <stdio.h>

#include <Vfw.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace Homer::Monitor;

#ifndef HWND_MESSAGE
#define HWND_MESSAGE                ((HWND)-3)
#endif

///////////////////////////////////////////////////////////////////////////////

MediaSourceVFW::MediaSourceVFW(string pDesiredDevice):
    MediaSource("VFW: local capture")
{
    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, PACKET_TYPE_RAW);

    mCurrentInputChannel = 0;
    mDesiredInputChannel = 0;

    bool tNewDeviceSelected = false;
    SelectDevice(pDesiredDevice, MEDIA_VIDEO, tNewDeviceSelected);
    if (!tNewDeviceSelected)
    {
        LOG(LOG_INFO, "Haven't selected new vfw device when creating source object");
    }
}

MediaSourceVFW::~MediaSourceVFW()
{
    if (mMediaSourceOpened)
        CloseGrabDevice();
}

void MediaSourceVFW::getVideoDevices(VideoDevicesList &pVList)
{
    static bool tFirstCall = true;
    HWND tWinHandle = NULL;
    int tRes;
    VideoDeviceDescriptor tDevice;

    #ifdef MSVFW_DEBUG_PACKETS
        tFirstCall = true;
    #endif

    if (tFirstCall)
    {
    	mFoundVFWDevices.clear();

        LOG(LOG_VERBOSE, "Enumerating hardware..");

		// windows supports up to 10 drivers which are indexed from 0 to 9
		//HINT: http://msdn.microsoft.com/en-us/library/dd756909%28VS.85%29.aspx
		char tDriverName[256];
		char tDriverVersion[32];
		for (int i = 0; i < 10; i++)
		{
			//####################################
			//### verbose output and store device description
			//####################################
			if (capGetDriverDescription(i, tDriverName, 256, tDriverVersion, 32))
			{
				LOG(LOG_INFO, "Found active VFW device %d", i);
				LOG(LOG_INFO, "  ..name: %s", tDriverName);
				LOG(LOG_INFO, "  ..version: %s", tDriverVersion);

				tDevice.Name = "VFW: " + string(tDriverName);
				tDevice.Card = (char)i + 48;
				tDevice.Desc = "VFW based video device " + tDevice.Card + " \"" + string(tDriverName) + "\"";
				LOG(LOG_VERBOSE, "Found video device: %s (card: %s)", tDevice.Name.c_str(), tDevice.Card.c_str());
			}

			//##############################################
			//### probe device by creating a capture window
			//##############################################
			tWinHandle = capCreateCaptureWindow(NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, 0);
			if(!tWinHandle)
			{
				LOG(LOG_INFO, "Could not create capture window");
				continue;
			}

			tRes = SendMessage(tWinHandle, WM_CAP_DRIVER_CONNECT, i, 0);
			if(!tRes)
			{
				LOG(LOG_INFO, "Could not connect to device");
				mDeviceAvailable[i] = false;
				DestroyWindow(tWinHandle);
				continue;
			}else
				mDeviceAvailable[i] = true;

			//HINT: maybe our capture frames are upside down, see http://www.microsoft.com/whdc/archive/biheight.mspx -> detect this
			BITMAPINFO tInfo;
			tRes = capGetVideoFormat(tWinHandle, &tInfo, sizeof(tInfo));
			if (!tRes)
			{
				LOG(LOG_ERROR, "Not connected to the capture window");
				DestroyWindow(tWinHandle);
				continue;
			}
			// see http://msdn.microsoft.com/de-de/library/dd183376.aspx
			mFramesAreUpsideDown[i] = (tInfo.bmiHeader.biHeight > 0);
			LOG(LOG_INFO, "  ..pictures are upside down: %d", mFramesAreUpsideDown[i]);

			DestroyWindow(tWinHandle);

			//###############################################
			//### finally add this device to the result list
			//###############################################
			mFoundVFWDevices.push_back(tDevice);
		}
    }else
    {
    	LOG(LOG_VERBOSE, "Using internal device cache with %d entries", (int)mFoundVFWDevices.size());
    }
    tFirstCall = false;

    VideoDevicesList::iterator tIt;
    for (tIt = mFoundVFWDevices.begin(); tIt != mFoundVFWDevices.end(); tIt++)
    {
    	pVList.push_back(*tIt);
    }
}

///////////////////////////////////////////////////////////////////////////////

bool MediaSourceVFW::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    int                 tResult;
    AVFormatParameters  tFormatParams;
    AVInputFormat       *tFormat;
    AVCodec             *tCodec;

    LOG(LOG_VERBOSE, "Trying to open the video source");

    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return false;
    }

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(VFW)");

    if (mMediaSourceOpened)
        return false;

    memset((void*)&tFormatParams, 0, sizeof(tFormatParams));
    tFormatParams.channel = 0;
    tFormatParams.standard = NULL;
    tFormatParams.time_base.num = 100;
    tFormatParams.time_base.den = (int)pFps * 100;
    LOG(LOG_VERBOSE, "Desired time_base: %d/%d (%3.2f)", tFormatParams.time_base.den, tFormatParams.time_base.num, pFps);
    tFormatParams.width = pResX;
    tFormatParams.height = pResY;
    tFormatParams.initial_pause = 0;
    tFormatParams.prealloced_context = 0;
    tFormat = av_find_input_format("vfwcap");
    if (tFormat == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find input format");
        return false;
    }

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
        mDesiredDevice = "";

    LOG(LOG_VERBOSE, "Going to open input device..");
    if (mDesiredDevice != "")
    {
        //########################################
        //### probing given device file
        //########################################
        tResult = 0;
        if ((tResult = av_open_input_file(&mFormatContext, (const char *)mDesiredDevice.c_str(), tFormat, 0, &tFormatParams)) != 0)
        {
            LOG(LOG_ERROR, "Couldn't open device \"%s\" because of \"%s\".", mDesiredDevice.c_str(), strerror(AVUNERROR(tResult)));
            return false;
        }
    }else
    {
        //########################################
        //### auto probing possible device files
        //########################################
        LOG(LOG_VERBOSE, "Auto-probing for VFW capture device");
        bool tFound = false;
        for (int i = 0; i < 10; i++)
        {
        	if (mDeviceAvailable[i])
        	{
        		LOG(LOG_VERBOSE, "Probing VFW device number: %d", i);
				mDesiredDevice = (char)i + 48;
				tResult = 0;
				if ((tResult = av_open_input_file(&mFormatContext, (const char *)mDesiredDevice.c_str(), tFormat, 0, &tFormatParams)) == 0)
				{
					LOG(LOG_VERBOSE, " ..available device connected");
					tFound = true;
					break;
				}
        	}
        }
        if (!tFound)
        {
            LOG(LOG_INFO, "Couldn't find a fitting VFW video device");
            return false;
        }
    }
    mCurrentDevice = mDesiredDevice;

    // Retrieve stream information
    LOG(LOG_VERBOSE, "Going to find stream information..");
    if ((tResult = av_find_stream_info(mFormatContext)) < 0)
    {
        LOG(LOG_ERROR, "Couldn't find stream information because of \"%s\".", strerror(tResult));
        // Close the VFW video file
        av_close_input_file(mFormatContext);
        return false;
    }

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
        // Close the VFW video file
        av_close_input_file(mFormatContext);
        return false;
    }

    //######################################################
    //### dump ffmpeg information about format
    //######################################################
    mFormatContext->streams[mMediaStreamIndex]->time_base.num = 100;
    mFormatContext->streams[mMediaStreamIndex]->time_base.den = (int)pFps * 100;

    // Dump information about device file
    dump_format(mFormatContext, mMediaStreamIndex, "MediaSourceVFW (video)", false);

    // Get a pointer to the codec context for the video stream
    mCodecContext = mFormatContext->streams[mMediaStreamIndex]->codec;

    // set grabbing resolution and frame-rate to the resulting ones delivered by opened video codec
    mSourceResX = mCodecContext->width;
    mSourceResY = mCodecContext->height;
    mFrameRate = (float)mFormatContext->streams[mMediaStreamIndex]->time_base.den / mFormatContext->streams[mMediaStreamIndex]->time_base.num;

    //######################################################
    //### search for correct decoder for the video stream
    //######################################################
    LOG(LOG_VERBOSE, "Going to find decoder..");
    if((tCodec = avcodec_find_decoder(mCodecContext->codec_id)) == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find a fitting codec");
        // Close the VFW video file
        av_close_input_file(mFormatContext);
        return false;
    }

    //######################################################
    //### open the selected codec
    //######################################################
    // Inform the codec that we can handle truncated bitstreams -- i.e.,
    // bitstreams where frame boundaries can fall in the middle of packets
//    if(tCodec->capabilities & CODEC_CAP_TRUNCATED)
//        mCodecContext->flags |= CODEC_FLAG_TRUNCATED;

    // Open codec
    LOG(LOG_VERBOSE, "Going to open codec..");
    if ((tResult = avcodec_open(mCodecContext, tCodec)) < 0)
    {
        LOG(LOG_ERROR, "Couldn't open codec because of \"%s\".", strerror(AVUNERROR(tResult)));
        // Close the VFW video file
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
    //av_seek_frame(mFormatContext, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->cur_dts, AVSEEK_FLAG_ANY);

    mFirstPixelformatError = true;
    mMediaType = MEDIA_VIDEO;
    MarkOpenGrabDeviceSuccessful();

    return true;
}

bool MediaSourceVFW::OpenAudioGrabDevice(int pSampleRate, bool pStereo)
{
    LOG(LOG_ERROR, "Wrong media type");
    return false;
}

bool MediaSourceVFW::CloseGrabDevice()
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
        StopRecording();

        mMediaSourceOpened = false;

        // free the software scaler context
        sws_freeContext(mScalerContext);

        // Close the VFW codec
        avcodec_close(mCodecContext);

		// Close the VFW video file
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

int MediaSourceVFW::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
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
        MarkGrabChunkFailed("video source is paused");

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
            MarkGrabChunkFailed("could'nt read next video frame");

            return -1;
        }
    }while (tPacket.stream_index != mMediaStreamIndex);

    if ((tPacket.data != NULL) && (tPacket.size > 0))
    {
        #ifdef MSVFW_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Grabbed new video packet:");
            LOG(LOG_VERBOSE, "      ..duration: %d", tPacket.duration);
            LOG(LOG_VERBOSE, "      ..pts: %ld stream [%d] pts: %ld", tPacket.pts, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->pts);
            LOG(LOG_VERBOSE, "      ..dts: %ld", tPacket.dts);
            LOG(LOG_VERBOSE, "      ..size: %d", tPacket.size);
            LOG(LOG_VERBOSE, "      ..pos: %ld", tPacket.pos);
        #endif

        // log statistics about original packets from device
        AnnouncePacket(tPacket.size);

        // decode packet and get a frame if the new frame is of any use for us
        if ((!pDropChunk) || (mRecording))
        {
            // Decode the next chunk of data
            tBytesDecoded = HM_avcodec_decode_video(mCodecContext, tSourceFrame, &tFrameFinished, &tPacket);

            // emulate set FPS
            tSourceFrame->pts = FpsEmulationGetPts();
//        // transfer the presentation time value
//        tSourceFrame->pts = tPacket.pts;

            #ifdef MSVFW_DEBUG_PACKETS
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

			if ((tFrameFinished) && (tBytesDecoded > 0))
			{
				// re-encode the frame and write it to file
				if (mRecording)
					RecordFrame(tSourceFrame);

				// convert frame to RGB format
				if (!pDropChunk)
				{
					HM_sws_scale(mScalerContext, tSourceFrame->data, tSourceFrame->linesize, 0, mCodecContext->height, tRGBFrame->data, tRGBFrame->linesize);
				}
			}else
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
bool MediaSourceVFW::SupportsRecording()
{
	return true;
}

string MediaSourceVFW::GetCodecName()
{
    return "Raw";
}

string MediaSourceVFW::GetCodecLongName()
{
    return "Raw";
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
