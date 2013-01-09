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
 * Purpose: Implementation of a DirectShow based local video source
 * Author:  Thomas Volkert
 * Since:   2012-08-16
 */

// the following source was inspired by:
//		"PlayCap" example from the Microsoft Windows SDK
//		http://msdn.microsoft.com/en-us/library/windows/desktop/dd407292%28v=vs.85%29.aspx
//		http://msdn.microsoft.com/en-us/library/windows/desktop/dd377566%28v=vs.85%29.aspx
// 		http://msdn.microsoft.com/en-us/library/windows/desktop/dd375620%28v=vs.85%29.aspx
//		http://sourceforge.net/tracker/?func=detail&atid=302435&aid=1819367&group_id=2435 - "Jan Wedekind"

#include <MediaSourceDShow.h>
#include <MediaSource.h>
#include <ProcessStatisticService.h>
#include <Logger.h>
#include <Header_Ffmpeg.h>

#include <cstdio>
#include <string.h>
#include <stdio.h>

#include <Header_DirectShow.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

MediaSourceDShow::MediaSourceDShow(string pDesiredDevice):
    MediaSource("DShow: local capture")
{
    mSourceType = SOURCE_DEVICE;
    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    bool tNewDeviceSelected = false;
    SelectDevice(pDesiredDevice, MEDIA_VIDEO, tNewDeviceSelected);
    if (!tNewDeviceSelected)
    {
        LOG(LOG_INFO, "Haven't selected new DirectShow device when creating source object");
    }
    LOG(LOG_VERBOSE, "Created");
}

MediaSourceDShow::~MediaSourceDShow()
{
    if (mMediaSourceOpened)
        CloseGrabDevice();
    LOG(LOG_VERBOSE, "Destroyed");
}

bool MediaSourceDShow::SupportsDecoderFrameStatistics()
{
    return (mMediaType == MEDIA_VIDEO);
}

void MediaSourceDShow::getVideoDevices(VideoDevices &pVList)
{
    static bool 			tFirstCall = true;
    VARIANT 				tVariant;
    TCHAR 					tDeviceName[256];
    TCHAR 					tDevicePath[256];
    TCHAR 					tDeviceDescription[256];
    VideoDeviceDescriptor 	tDevice;
    HRESULT 				tRes = S_OK;
    IMoniker				*tMoniker =NULL;
    ICreateDevEnum 			*tDeviceEnumerator =NULL;
    IEnumMoniker 			*tClassEnumerator = NULL;
    CLSID 					tClassId;
    bool					tDeviceIsVFW;
    bool					tDeviceIsUsable;
    IBaseFilter				*tSource;
	IPropertyBag 			*tPropertyBag;
	IAMStreamConfig 		*tStreamConfig = NULL;
	IEnumPins  				*tPinsEnum = NULL;
	IPin					*tPin = NULL;
	PIN_DIRECTION			tPinDir;

	#ifdef MSDS_DEBUG_PACKETS
		tFirstCall = true;
	#endif

	if (tFirstCall)
		LOG(LOG_VERBOSE, "Enumerating hardware..");

    VariantInit(&tVariant);

    // create system device enumerator
    tRes = CoCreateInstance (CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, IID_ICreateDevEnum, (void **) &tDeviceEnumerator);
    if (FAILED(tRes))
    {
    	LOG(LOG_ERROR, "Could not create the Windows system device enumerator");
    	return;
    }

    // create enumerator capture devices
    tRes = tDeviceEnumerator->CreateClassEnumerator (CLSID_VideoInputDeviceCategory, &tClassEnumerator, 0);
	if (FAILED(tRes))
	{
    	LOG(LOG_ERROR, "Could not create the Windows enumerator for video input devices");
    	return;
    }

	if (tClassEnumerator == NULL)
	{
		LOG(LOG_VERBOSE, "No DirectShow video capture device was detected");
		return;
	}

	int tCount = -1;
	// access video input device
	while(tClassEnumerator->Next (1, &tMoniker, NULL) == S_OK)
	{
		tDeviceIsUsable = false;
		memset(tDeviceName, 0, sizeof(tDeviceName));
		memset(tDeviceDescription, 0, sizeof(tDeviceDescription));
		memset(tDevicePath, 0, sizeof(tDevicePath));
		tCount++;

		// bind to storage
		tRes = tMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&tPropertyBag);
		if (FAILED(tRes))
		{
			LOG(LOG_ERROR, "Could not bind moniker to storage object");
			continue;
		}

		// retrieve the device's name
		tRes = tPropertyBag->Read(L"FriendlyName", &tVariant, 0);
		if (FAILED(tRes))
		{
			LOG(LOG_ERROR, "Could not get property \"FriendlyName\" for device \"%d\"", tCount);
			tPropertyBag->Release();
			continue;
		}
		WideCharToMultiByte(CP_ACP, 0, tVariant.bstrVal, -1, tDeviceName, sizeof(tDeviceName), 0, 0);
		VariantClear(&tVariant);

		// retrieve the device's description
		tRes = tPropertyBag->Read(L"Description", &tVariant, 0);
		if (SUCCEEDED(tRes))
			WideCharToMultiByte(CP_ACP, 0, tVariant.bstrVal, -1, tDeviceDescription, sizeof(tDeviceDescription), 0, 0);
		else
			LOG(LOG_WARN, "Could not get property \"Description\" for device \"%d\"", tCount);
		VariantClear(&tVariant);

		// retrieve the device's description
		tRes = tPropertyBag->Read(L"DevicePath", &tVariant, 0);
		if (SUCCEEDED(tRes))
			WideCharToMultiByte(CP_ACP, 0, tVariant.bstrVal, -1, tDevicePath, sizeof(tDevicePath), 0, 0);
		else
			LOG(LOG_WARN, "Could not get property \"DevicePath\" for device \"%d\"", tCount);
		VariantClear(&tVariant);


		// bint to object
		string tDeviceNameStr = string(tDeviceName);
		LOG(LOG_VERBOSE, "Will try to detect supported video resolutions for device \"%s\"", tDeviceNameStr.c_str());
		tMoniker->BindToObject( 0, 0, IID_IBaseFilter, (void **)&tSource);

	    tRes = tSource->GetClassID(&tClassId);
	    if (tClassId == CLSID_VfwCapture)
	    	tDeviceIsVFW = true;
	    else
	    	tDeviceIsVFW = false;

		//####################################
		//### verbose output and store device description
		//####################################
		tDevice.Name = string(tDeviceName);
		tDevice.Card = "video=" + string(tDeviceName);
		tDevice.Desc = "DirectShow based video device \"" + string(tDeviceName) + "\"";
		tDevice.Type = Camera; // assume all as camera devices

        if (tFirstCall)
        {
			LOG(LOG_INFO, "Found active DirectShow device %d", tCount);
			LOG(LOG_INFO, "  ..device name: %s", tDeviceName);
			LOG(LOG_INFO, "  ..description: %s", tDeviceDescription);
			LOG(LOG_INFO, "  ..path: %s", tDevicePath);
			LOG(LOG_INFO, "  ..type: %s", tDeviceIsVFW ? "VFW device" : "DirectShow device");
        }

        // enumerate all supported video resolutions
		tRes = tSource->EnumPins(&tPinsEnum);
		if (FAILED(tRes))
		{
			LOG(LOG_ERROR, "Could not enumerate pins");
			tPropertyBag->Release();
			break;
		}

		while (tPinsEnum->Next(1, &tPin, 0) == S_OK)
		{
			tRes = tPin->QueryDirection(&tPinDir);
			if (FAILED(tRes))
			{
				LOG(LOG_ERROR, "Could not query pin direction");
				break;
			}

			if (tPinDir == PINDIR_OUTPUT)
			{
				tRes = tPin->QueryInterface(IID_IAMStreamConfig, (void **)&tStreamConfig);
				if (FAILED(tRes))
				{
					LOG(LOG_ERROR, "Could not query pin interface");
					tPin->Release();
					break;
				}

				int tCount, tSize;
				tRes = tStreamConfig->GetNumberOfCapabilities( &tCount, &tSize);
				LOG(LOG_VERBOSE, "Found %d capability entries", tCount);
				if (FAILED(tRes))
				{
					LOG(LOG_ERROR, "Could not get number of caps");
					tPin->Release();
					break;
				}

				AM_MEDIA_TYPE *tMT;
				BYTE *tCaps = new BYTE[tSize]; // VIDEO_STREAM_CONFIG_CAPS

				for ( int i = 0; i < tCount; i++)
				{
					tRes = tStreamConfig->GetStreamCaps(i, &tMT, tCaps);
					if (FAILED(tRes))
					{
						#ifdef MSDS_DEBUG_RESOLUTIONS
							LOG(LOG_ERROR, "Could not get stream cap %d", i);
						#endif
						break;
					}

					if ((tMT->majortype == MEDIATYPE_Video) && (tMT->pbFormat !=0))
					{
						int tFormatType = -1;
						int tWidth = -1;
						int tHeight = -1;
						if (tMT->formattype == FORMAT_VideoInfo)
						{// VideoInfo
							VIDEOINFOHEADER *tVInfoHeader = (VIDEOINFOHEADER*)tMT->pbFormat;
							tWidth = tVInfoHeader->bmiHeader.biWidth;
							tHeight = tVInfoHeader->bmiHeader.biHeight;
							tFormatType = 1;
						}else if (tMT->formattype == FORMAT_VideoInfo2)
						{// VideoInfo2
							VIDEOINFOHEADER2 *tVInfoHeader2 = (VIDEOINFOHEADER2*)tMT->pbFormat;
							tWidth = tVInfoHeader2->bmiHeader.biWidth;
							tHeight = tVInfoHeader2->bmiHeader.biHeight;
							tFormatType = 2;
						}else
						{
							if (tFirstCall)
								LOG(LOG_WARN, "Unsupported format type detected: %s", GUID2String(tMT->formattype).c_str());
						}
						tDeviceIsUsable = true;

						if (tFirstCall)
							LOG(LOG_VERBOSE, "  ..supported media tyoe: Video, video info format: %d, video resolution: %d*%d (sub type: %s)", tFormatType, tWidth, tHeight, GetSubTypeName(tMT->subtype).c_str());
					}else
					{
						if (tFirstCall)
							LOG(LOG_VERBOSE, "  ..additional media type: %s", GetMediaTypeName(tMT->majortype).c_str());
					}
				}
				delete [] tCaps;
			}
			tPin->Release();
		}
		tPinsEnum->Release();

        //###############################################
		//### finally add this device to the result list
		//###############################################
		if (tDeviceIsUsable)
			pVList.push_back(tDevice);
		else
			LOG(LOG_WARN, "Ignoring device %s", tDevice.Name.c_str());

		tPropertyBag->Release();
		tMoniker->Release();
	}
	tClassEnumerator->Release();

    tFirstCall = false;
}

///////////////////////////////////////////////////////////////////////////////

bool MediaSourceDShow::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
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

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(DShow)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    av_dict_set(&tOptions, "video_size", (toString(pResX) + "x" + toString(pResY)).c_str(), 0);
    av_dict_set(&tOptions, "framerate", toString((int)pFps).c_str(), 0);

    tFormat = av_find_input_format("dshow");
    if (tFormat == NULL)
    {
        LOG(LOG_ERROR, "Couldn't find input format");
        return false;
    }

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
        mDesiredDevice = "";

    // alocate new format context
    mFormatContext = AV_NEW_FORMAT_CONTEXT();

    LOG(LOG_VERBOSE, "Going to open input device \"%s\" with resolution %d*%d..", mDesiredDevice.c_str(), pResX, pResY);
    if (mDesiredDevice != "")
    {
        //########################################
        //### probing given device file
        //########################################
        if ((tResult = avformat_open_input(&mFormatContext, (const char *)mDesiredDevice.c_str(), tFormat, &tOptions)) != 0)
        {
            if (tResult != 0)
            {
				LOG(LOG_WARN, "Device open failed - switching back to default values");

				av_dict_free(&tOptions);
				tOptions = NULL;

				//av_dict_set(&tOptions, "video_size", "352x288", 0);
				tResult = avformat_open_input(&mFormatContext, (const char *)mDesiredDevice.c_str(), tFormat, &tOptions);
            }
            if (tResult != 0)
            {
        		LOG(LOG_ERROR, "Couldn't open device \"%s\" because of \"%s\".", mDesiredDevice.c_str(), strerror(AVUNERROR(tResult)));
        		return false;
            }
        }
    }else
    {
        //########################################
        //### auto probing possible device files
        //########################################
        LOG(LOG_VERBOSE, "Auto-probing for DShow capture device");
        VideoDevices tDevices;
        getVideoDevices(tDevices);

        bool tFound = false;
        for (int i = 0; i < (int)tDevices.size(); i++)
        {
			LOG(LOG_VERBOSE, "Probing DShow device number: %d", i);
			mDesiredDevice = tDevices[i].Card;
			if ((tResult = avformat_open_input(&mFormatContext, (const char *)mDesiredDevice.c_str(), tFormat, &tOptions)) == 0)
			{
				LOG(LOG_VERBOSE, " ..available device connected");
				tFound = true;
				break;
			}
        }
        if (!tFound)
        {
            LOG(LOG_INFO, "Couldn't find a fitting DShow video device");
            return false;
        }
    }

    av_dict_free(&tOptions);

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
    if ((mSourceFrame = avcodec_alloc_frame()) == NULL)
        return false;
    if ((mRGBFrame = avcodec_alloc_frame()) == NULL)
        return false;

    MarkOpenGrabDeviceSuccessful();

    return true;
}

bool MediaSourceDShow::OpenAudioGrabDevice(int pSampleRate, int pChannels)
{
    LOG(LOG_ERROR, "Wrong media type");
    return false;
}

bool MediaSourceDShow::CloseGrabDevice()
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

int MediaSourceDShow::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
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
        #ifdef MSDS_DEBUG_PACKETS
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
            tBytesDecoded = HM_avcodec_decode_video(mCodecContext, mSourceFrame, &tFrameFinished, &tPacket);

            // emulate set FPS
            mSourceFrame->pts = GetPtsFromFpsEmulator();

//        // transfer the presentation time value
//        mSourceFrame->pts = tPacket.pts;

            #ifdef MSDS_DEBUG_PACKETS
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
                LOG(LOG_VERBOSE, "      ..pts: %ld", mSourceFrame->pts);
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

GrabResolutions MediaSourceDShow::GetSupportedVideoGrabResolutions()
{
    static bool 			tFirstCall = true;
    VARIANT 				tVariant;
    TCHAR 					tDeviceName[256];
    TCHAR 					tDevicePath[256];
    TCHAR 					tDeviceDescription[256];
    VideoDeviceDescriptor 	tDevice;
    HRESULT 				tRes = S_OK;
    IMoniker				*tMoniker =NULL;
    ICreateDevEnum 			*tDeviceEnumerator =NULL;
    IEnumMoniker 			*tClassEnumerator = NULL;
    CLSID 					tClassId;
    bool					tDeviceIsVFW;
    IBaseFilter				*tSource;
	IPropertyBag 			*tPropertyBag;
	VideoFormatDescriptor 	tFormat;
	IAMStreamConfig 		*tStreamConfig = NULL;
	IEnumPins  				*tPinsEnum = NULL;
	IPin					*tPin = NULL;
	PIN_DIRECTION			tPinDir;

    if (mMediaType != MEDIA_VIDEO)
    {
    	LOG(LOG_ERROR, "Invalid media type");
		return mSupportedVideoFormats;
    }

    if (mCurrentDeviceName == "")
    {
    	LOG(LOG_ERROR, "Current device name is empty");
		return mSupportedVideoFormats;
    }

    mSupportedVideoFormats.clear();

    VariantInit(&tVariant);

    // create system device enumerator
    tRes = CoCreateInstance (CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, IID_ICreateDevEnum, (void **) &tDeviceEnumerator);
    if (FAILED(tRes))
    {
    	LOG(LOG_ERROR, "Could not create the Windows system device enumerator");
    	return mSupportedVideoFormats;
    }

    // create enumerator capture devices
    tRes = tDeviceEnumerator->CreateClassEnumerator (CLSID_VideoInputDeviceCategory, &tClassEnumerator, 0);
	if (FAILED(tRes))
	{
    	LOG(LOG_ERROR, "Could not create the Windows enumerator for video input devices");
    	return mSupportedVideoFormats;
    }

	if (tClassEnumerator == NULL)
	{
		LOG(LOG_VERBOSE, "No DirectShow video capture device was detected");
		return mSupportedVideoFormats;
	}

	int tCount = -1;
	// access video input device
	while(tClassEnumerator->Next (1, &tMoniker, NULL) == S_OK)
	{
		memset(tDeviceName, 0, sizeof(tDeviceName));
		memset(tDeviceDescription, 0, sizeof(tDeviceDescription));
		memset(tDevicePath, 0, sizeof(tDevicePath));
		tCount++;

		// bind to storage
		tRes = tMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&tPropertyBag);
		if (FAILED(tRes))
		{
			LOG(LOG_ERROR, "Could not bind moniker to storage object");
			continue;
		}

		// retrieve the device's name
		tRes = tPropertyBag->Read(L"FriendlyName", &tVariant, 0);
		if (FAILED(tRes))
		{
			LOG(LOG_ERROR, "Could not get property \"FriendlyName\" for device %d", tCount);
			tPropertyBag->Release();
			continue;
		}
		WideCharToMultiByte(CP_ACP, 0, tVariant.bstrVal, -1, tDeviceName, sizeof(tDeviceName), 0, 0);
		VariantClear(&tVariant);

		// have we found our current device in the enumeration?
		string tDeviceNameStr = string(tDeviceName);
		LOG(LOG_VERBOSE, "Comparing %s and %s for video resolution detection", tDeviceNameStr.c_str(), mCurrentDeviceName.c_str());
		if (tDeviceNameStr == mCurrentDeviceName)
		{
			LOG(LOG_VERBOSE, "Will try to detect supported video resolutions for device %s", tDeviceNameStr.c_str());
			// bint to object
			tMoniker->BindToObject( 0, 0, IID_IBaseFilter, (void **)&tSource);

			tRes = tSource->EnumPins(&tPinsEnum);
			if (FAILED(tRes))
			{
				LOG(LOG_ERROR, "Could not enumerate pins");
				tPropertyBag->Release();
				break;
			}

			while (tPinsEnum->Next(1, &tPin, 0) == S_OK)
			{
				tRes = tPin->QueryDirection(&tPinDir);
				if (FAILED(tRes))
				{
					LOG(LOG_ERROR, "Could not query pin direction");
					break;
				}

				if (tPinDir == PINDIR_OUTPUT)
				{
					tRes = tPin->QueryInterface(IID_IAMStreamConfig, (void **)&tStreamConfig);
					if (FAILED(tRes))
					{
						LOG(LOG_ERROR, "Could not query pin interface");
						tPin->Release();
						break;
					}

					int tCount, tSize;
					tRes = tStreamConfig->GetNumberOfCapabilities( &tCount, &tSize);
					LOG(LOG_VERBOSE, "Found %d capability entries", tCount);
					if (FAILED(tRes))
					{
						LOG(LOG_ERROR, "Could not get number of caps");
						tPin->Release();
						break;
					}

			    	AM_MEDIA_TYPE *tMT;
			    	BYTE *tCaps = new BYTE[tSize]; // VIDEO_STREAM_CONFIG_CAPS

			    	for ( int i = 0; i < tCount; i++)
				    {
			    		tRes = tStreamConfig->GetStreamCaps(i, &tMT, tCaps);
						if (FAILED(tRes))
						{
							#ifdef MSDS_DEBUG_RESOLUTIONS
								LOG(LOG_ERROR, "Could not get stream cap %d", i);
							#endif
							break;
						}

					    if ((tMT->majortype == MEDIATYPE_Video) && (tMT->pbFormat !=0))
					    {
							int tWidth = -1;
							int tHeight = -1;
					    	if (tMT->formattype == FORMAT_VideoInfo)
					    	{// VideoInfo
					    		VIDEOINFOHEADER *tVInfoHeader = (VIDEOINFOHEADER*)tMT->pbFormat;
								tWidth = tVInfoHeader->bmiHeader.biWidth;
								tHeight = tVInfoHeader->bmiHeader.biHeight;
					    	}else if (tMT->formattype == FORMAT_VideoInfo2)
					    	{// VideoInfo2
					    		VIDEOINFOHEADER2 *tVInfoHeader2 = (VIDEOINFOHEADER2*)tMT->pbFormat;
								tWidth = tVInfoHeader2->bmiHeader.biWidth;
								tHeight = tVInfoHeader2->bmiHeader.biHeight;
					    	}else
					    	{
					    		LOG(LOG_WARN, "Unsupported format type detected: %s", GUID2String(tMT->formattype).c_str());
					    	}

							#ifdef MSDS_DEBUG_RESOLUTIONS
								LOG(LOG_VERBOSE, "Detected source video resolution %d*%d and sub type: %s", tWidth, tHeight, GetSubTypeName(tMT->subtype).c_str());
							#endif
							if ((tWidth <= 1920) && (tHeight <= 1080))
							{
								tFormat.Name= "";
								tFormat.ResX = tWidth;
								tFormat.ResY = tHeight;
								bool tAlreadyKnownResolution = false;
								for (int i = 0; i < (int)mSupportedVideoFormats.size(); i++)
								{
									if ((mSupportedVideoFormats[i].ResX == tWidth) && (mSupportedVideoFormats[i].ResY == tHeight))
									{
										tAlreadyKnownResolution = true;
										break;
									}
								}
								if (!tAlreadyKnownResolution)
								{
		                            if ((tHeight > 0) && (tWidth > 0))
		                            {
		                                LOG(LOG_VERBOSE, "Adding resolution %d*%d to the supported ones", tWidth, tHeight);
		                                mSupportedVideoFormats.push_back(tFormat);
		                            }
								}
							}
						}
				    }
			    	delete [] tCaps;
				}
				tPin->Release();
			}
			tPinsEnum->Release();
		}

		tPropertyBag->Release();
		tMoniker->Release();
	}
	tClassEnumerator->Release();

    return mSupportedVideoFormats;
}

bool MediaSourceDShow::SupportsRecording()
{
	return true;
}

string MediaSourceDShow::GetCodecName()
{
    return "Raw";
}

string MediaSourceDShow::GetCodecLongName()
{
    return "Raw";
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
