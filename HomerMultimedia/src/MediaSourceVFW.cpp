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
#include <Header_Ffmpeg.h>

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
    mSourceType = SOURCE_DEVICE;
    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

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

bool MediaSourceVFW::SupportsDecoderFrameStatistics()
{
    return (mMediaType == MEDIA_VIDEO);
}

void MediaSourceVFW::getVideoDevices(VideoDevices &pVList)
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

				tDevice.Name = string(tDriverName);
				tDevice.Card = (char)i + 48;
				tDevice.Desc = "VFW based video device " + tDevice.Card + " \"" + string(tDriverName) + "\"";
				tDevice.Type = Camera; // assume all as camera devices
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

    VideoDevices::iterator tIt;
    for (tIt = mFoundVFWDevices.begin(); tIt != mFoundVFWDevices.end(); tIt++)
    {
    	pVList.push_back(*tIt);
    }
}

///////////////////////////////////////////////////////////////////////////////

bool MediaSourceVFW::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    int                 tResult;
    AVDictionary        *tOptions = NULL;
    AVInputFormat       *tFormat;
    AVCodec             *tCodec;

    mMediaType = MEDIA_VIDEO;

    if (pFps > 29.97)
        pFps = 29.97;
    if (pFps < 5)
        pFps = 5;

    LOG(LOG_VERBOSE, "Trying to open the video source");

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(VFW)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    av_dict_set(&tOptions, "video_size", (toString(pResX) + "x" + toString(pResY)).c_str(), 0);
    av_dict_set(&tOptions, "framerate", toString((int)pFps).c_str(), 0);

    tFormat = av_find_input_format("vfwcap");
    if (tFormat == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find input format");
        return false;
    }

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
        mDesiredDevice = "";

    // alocate new format context
    mFormatContext = AV_NEW_FORMAT_CONTEXT();

    LOG(LOG_VERBOSE, "Going to open input device..");
    if (mDesiredDevice != "")
    {
        //########################################
        //### probing given device file
        //########################################
        if ((tResult = avformat_open_input(&mFormatContext, (const char *)mDesiredDevice.c_str(), tFormat, &tOptions)) != 0)
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
				if ((tResult = avformat_open_input(&mFormatContext, (const char *)mDesiredDevice.c_str(), tFormat, &tOptions)) == 0)
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

    if (!DetectAllStreams())
    	return false;

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
    //av_seek_frame(mFormatContext, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->cur_dts, AVSEEK_FLAG_ANY);

    // Allocate video frame for source and RGB format
    if ((mSourceFrame = AllocFrame()) == NULL)
        return false;
    if ((mRGBFrame = AllocFrame()) == NULL)
        return false;

    MarkOpenGrabDeviceSuccessful();

    return true;
}

bool MediaSourceVFW::OpenAudioGrabDevice(int pSampleRate, int pChannels)
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
        CloseAll();

        // Free the frames
        av_free(mRGBFrame);
        av_free(mSourceFrame);

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
        MarkGrabChunkFailed("video source is paused");

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
            MarkGrabChunkFailed("could'nt read next video frame");

            return GRAB_RES_INVALID;
        }
    }while (tPacket.stream_index != mMediaStreamIndex);

    if ((tPacket.data != NULL) && (tPacket.size > 0))
    {
        #ifdef MSVFW_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Grabbed new video packet:");
            LOG(LOG_VERBOSE, "      ..duration: %d", tPacket.duration);
            LOG(LOG_VERBOSE, "      ..pts: %"PRId64" stream [%d] pts: %"PRId64"", tPacket.pts, mMediaStreamIndex, mFormatContext->streams[mMediaStreamIndex]->pts);
            LOG(LOG_VERBOSE, "      ..dts: %"PRId64"", tPacket.dts);
            LOG(LOG_VERBOSE, "      ..size: %d", tPacket.size);
            LOG(LOG_VERBOSE, "      ..pos: %"PRId64"", tPacket.pos);
        #endif

        // log statistics about original packets from device
        AnnouncePacket(tPacket.size);

        // decode packet and get a frame if the new frame is of any use for us
        if ((!pDropChunk) || (mRecording))
        {
            // Decode the next chunk of data
            tBytesDecoded = HM_avcodec_decode_video(mCodecContext, mSourceFrame, &tFrameFinished, &tPacket);

            // emulate set FPS
            mSourceFrame->pts = GetPtsFromFpsEmulator();

//        // transfer the presentation time value
//        mSourceFrame->pts = tPacket.pts;

            #ifdef MSVFW_DEBUG_PACKETS
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

			if ((tFrameFinished) && (tBytesDecoded > 0))
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

bool MediaSourceVFW::SupportsRecording()
{
	return true;
}

string MediaSourceVFW::GetSourceCodecStr()
{
    return "Raw";
}

string MediaSourceVFW::GetSourceCodecDescription()
{
    return "Raw";
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
