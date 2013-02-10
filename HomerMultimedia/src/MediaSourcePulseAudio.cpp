/*****************************************************************************
 *
 * Copyright (C) 2013 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: PulseAudio capture implementation
 * Author:  Thomas Volkert
 * Since:   2013-02-09
 */

#include <Header_PulseAudio.h>
#include <MediaFifo.h>
#include <MediaSourcePulseAudio.h>
#include <ProcessStatisticService.h>
#include <Logger.h>
#include <HBThread.h>

#include <string.h>
#include <stdlib.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////
////////////////////// DEVICE ENUMERATION /////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// The following was developed based on http://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/Samples/AsyncDeviceList

// this callback gets called when the context changes state
void CallbackStateChange(pa_context *pContext, void *pUserdata)
{
        pa_context_state_t tState;
        int *tIsReady = (int*)pUserdata;

        tState = pa_context_get_state(pContext);
        switch  (tState)
        {
                case PA_CONTEXT_READY:
                        *tIsReady = 1;
                        break;
                case PA_CONTEXT_FAILED:
                case PA_CONTEXT_TERMINATED:
                        *tIsReady = 2;
                        break;
				// there are just here for reference
				case PA_CONTEXT_UNCONNECTED:
				case PA_CONTEXT_CONNECTING:
				case PA_CONTEXT_AUTHORIZING:
				case PA_CONTEXT_SETTING_NAME:
				default:
						break;
        }
}

// pa_mainloop will call this function when it's ready to tell us about a sink
void CallbackAudioSinksList(pa_context *c, const pa_sink_info *pSinkInfo, int pEol, void *pUserdata)
{
    PulseAudioDeviceDescriptor *tDevicesList = (PulseAudioDeviceDescriptor*)pUserdata;

    // If eol is set to a positive number, you're at the end of the list
    if (pEol > 0)
    {
        return;
    }

    // We know we've allocated MAX_PULSEAUDIO_DEVICES_IN_LIST slots to hold devices.  Loop through our
    // structure and find the first one that's "uninitialized."  Copy the
    // contents into it and we're done.  If we receive more than 16 devices,
    // they're going to get dropped.  You could make this dynamically allocate
    // space for the device list, but this is a simple example.
    for (int i = 0; i < MAX_PULSEAUDIO_DEVICES_IN_LIST; i++)
    {
        if (!tDevicesList[i].Initialized)
        {
            strncpy(tDevicesList[i].Name, pSinkInfo->name, 511);
            strncpy(tDevicesList[i].Description, pSinkInfo->description, 255);
            tDevicesList[i].Index = pSinkInfo->index;
            tDevicesList[i].Initialized = 1;
            break;
        }
    }
}

// pa_mainloop will call this function when it's ready to tell us about a source
void CallbackAudioSourcesList(pa_context *pContext, const pa_source_info *pSourceInfo, int pEol, void *pUserdata)
{
    PulseAudioDeviceDescriptor *tDeviceList = (PulseAudioDeviceDescriptor*)pUserdata;

    if (pEol > 0)
    {
        return;
    }

    LOGEX(MediaSourcePulseAudio, LOG_INFO, "Detected PulseAudio source device..");
    LOGEX(MediaSourcePulseAudio, LOG_INFO, "..name: %s", pSourceInfo->name);
    LOGEX(MediaSourcePulseAudio, LOG_INFO, "..index: %u", pSourceInfo->index);
    LOGEX(MediaSourcePulseAudio, LOG_INFO, "..description: %s", pSourceInfo->description);
    LOGEX(MediaSourcePulseAudio, LOG_INFO, "..monitor_of_sink: %u", pSourceInfo->monitor_of_sink);

    // we don't want to use monitors
    if (pSourceInfo->monitor_of_sink != PA_INVALID_INDEX)
    	return;

    for (int i = 0; i < MAX_PULSEAUDIO_DEVICES_IN_LIST; i++)
    {
        if (!tDeviceList[i].Initialized)
        {
            strncpy(tDeviceList[i].Name, pSourceInfo->name, 511);
            strncpy(tDeviceList[i].Description, pSourceInfo->description, 255);
            tDeviceList[i].Index = pSourceInfo->index;
            tDeviceList[i].Initialized = 1;
            break;
        }
    }
}

int MediaSourcePulseAudio::GetPulseAudioDevices(PulseAudioDeviceDescriptor *pInputDevicesList, PulseAudioDeviceDescriptor *pOutputDevicesList)
{
    pa_mainloop *tMainLoop;
    pa_mainloop_api *tMainLoopAPi;
    pa_operation *tOperation;
    pa_context *tContext;

    // state variables to keep track of the requests
    int tState = 0;
    int tIsReady = 0;

    // initialize device lists
    memset(pInputDevicesList, 0, sizeof(PulseAudioDeviceDescriptor) * MAX_PULSEAUDIO_DEVICES_IN_LIST);
    memset(pOutputDevicesList, 0, sizeof(PulseAudioDeviceDescriptor) * MAX_PULSEAUDIO_DEVICES_IN_LIST);

    // create a mainloop API and connection to the default server
    tMainLoop = pa_mainloop_new();
    tMainLoopAPi = pa_mainloop_get_api(tMainLoop);
    tContext = pa_context_new(tMainLoopAPi, "enumeration");

    // this function connects to the pulse server
    pa_context_connect(tContext, NULL, PA_CONTEXT_NOFLAGS, NULL);

    // define a callback so the server will tell us its state
    // the callback will wait for the state "ready"
    // the callback modifies the variable to 1 and signals a connection is established and ready
    // if there's an error, the callback will set tIsReady to 2
    pa_context_set_state_callback(tContext, CallbackStateChange, &tIsReady);

    // enter a loop until all data is received or an error occurred
    for (;;)
    {
        // wait until PA is ready, otherwise iterate the mainloop and continue
        if (tIsReady == 0)
        {
            pa_mainloop_iterate(tMainLoop, 1, NULL);
            continue;
        }

        // connection to the server not possible
        if (tIsReady == 2)
        {
            pa_context_disconnect(tContext);
            pa_context_unref(tContext);
            pa_mainloop_free(tMainLoop);
            return -1;
        }

        // connection established to the server and ready for requests
        switch (tState)
        {
            // state 0: nothing done yet
            case 0:
                // send request to server, the operation ID is stored in the tOperation variable
                tOperation = pa_context_get_sink_info_list(tContext, CallbackAudioSinksList, pOutputDevicesList);

                // update state for next iteration through the loop
                tState++;
                break;
            case 1:
                // wait for completion of operation
                if (pa_operation_get_state(tOperation) == PA_OPERATION_DONE)
                {
                    pa_operation_unref(tOperation);

                    // send another request to server in order to get the sources (input devices) list
                    tOperation = pa_context_get_source_info_list(tContext, CallbackAudioSourcesList, pInputDevicesList);

                    // update the state to know what to do next
                    tState++;
                }
                break;
            case 2:
                if (pa_operation_get_state(tOperation) == PA_OPERATION_DONE)
                {
                    // everything is done, clean up & disconnect & return
                    pa_operation_unref(tOperation);
                    pa_context_disconnect(tContext);
                    pa_context_unref(tContext);
                    pa_mainloop_free(tMainLoop);
                    return 0;
                }
                break;
            default:
                // this state should never be reached
                LOGEX(WaveOutPulseAudio, LOG_ERROR, "Unexpected state %d", tState);
                return -1;
        }
        // iterate the main loop and go again,
        pa_mainloop_iterate(tMainLoop, 1 /* 0 means non-blocking */, NULL);
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

static int sPulseAudioServerAvailable = -1;

bool MediaSourcePulseAudio::PulseAudioAvailable()
{
	pa_sample_spec 	tOutputFormat;
	int				tRes;
	pa_simple 		*tOutputStream = NULL;
	bool			tResult;

	if (sPulseAudioServerAvailable == -1)
	{
		tOutputFormat.format = PA_SAMPLE_S16LE;
		tOutputFormat.rate = 44100;
		tOutputFormat.channels = 2;

		LOGEX(WaveOutPulseAudio, LOG_VERBOSE, "Probing PulseAudio server..");
		if (!(tOutputStream = pa_simple_new(NULL, "Homer-Conferencing", PA_STREAM_PLAYBACK, NULL /* dev Name */, "test playback", &tOutputFormat, NULL, NULL, &tRes)))
		{
			LOGEX(WaveOutPulseAudio, LOG_WARN, "Couldn't create PulseAudio stream because %s(%d)", pa_strerror(tRes), tRes);
			sPulseAudioServerAvailable = false;
		}else
		{
			LOGEX(WaveOutPulseAudio, LOG_VERBOSE, ">>> PulseAudio available <<<");
			sPulseAudioServerAvailable = true;
		}

		if (tOutputStream != NULL)
			pa_simple_free(tOutputStream);
	}

	tResult = sPulseAudioServerAvailable;

	return tResult;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

MediaSourcePulseAudio::MediaSourcePulseAudio(string pDesiredDevice):
    MediaSource("PulseAudio: local capture")
{
    mSourceType = SOURCE_DEVICE;
    ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);

    if (pDesiredDevice != "")
    {
        bool tNewDeviceSelected = false;
        SelectDevice(pDesiredDevice, MEDIA_AUDIO, tNewDeviceSelected);
        if (!tNewDeviceSelected)
            LOG(LOG_INFO, "Haven't selected new PulseAudio device when creating source object");
    }

    LOG(LOG_VERBOSE, "Created");
}

MediaSourcePulseAudio::~MediaSourcePulseAudio()
{
    LOG(LOG_VERBOSE, "Destroying PulseAudio grabber");

    StopGrabbing();

    if (mMediaSourceOpened)
        CloseGrabDevice();

    LOG(LOG_VERBOSE, "Destroyed");
}

void MediaSourcePulseAudio::getAudioDevices(AudioDevices &pAList)
{
	// This is where we'll store the input device list
	PulseAudioDeviceDescriptor tInputDevicesList[MAX_PULSEAUDIO_DEVICES_IN_LIST];

	// This is where we'll store the output device list
	PulseAudioDeviceDescriptor tOutputDevicesList[MAX_PULSEAUDIO_DEVICES_IN_LIST];

	if (MediaSourcePulseAudio::GetPulseAudioDevices(tInputDevicesList, tOutputDevicesList) < 0)
		LOG(LOG_ERROR, "Couldn't determine the available PulseAudio devices");

    static bool tFirstCall = true;
    AudioDeviceDescriptor tDevice;

    #ifdef MSPUA_DEBUG_PACKETS
        tFirstCall = true;
    #endif

    if (tFirstCall)
    {
        LOG(LOG_VERBOSE, "Enumerating hardware..");
        LOG(LOG_VERBOSE, "PulseAudio version \"%s\"(%d.%d.%d)", pa_get_library_version(), PA_MAJOR, PA_MINOR, PA_MICRO);
    }

    for (int i = 0; i < MAX_PULSEAUDIO_DEVICES_IN_LIST; i++)
    {
        if (!tInputDevicesList[i].Initialized)
            break;

        tDevice.Name = string(tInputDevicesList[i].Description);
        tDevice.Card = string(tInputDevicesList[i].Name);
        tDevice.Desc = "PulseAudio based audio device";
        tDevice.IoType = "Input";
        pAList.push_back(tDevice);

        if (tFirstCall)
        {
            LOG(LOG_VERBOSE, "Input device %d..", i);
            LOG(LOG_VERBOSE, "..name: \"%s\"", tDevice.Name.c_str());
            LOG(LOG_VERBOSE, "..card: \"%s\"", tDevice.Card.c_str());
            LOG(LOG_VERBOSE, "..description: \"%s\"", tDevice.Desc.c_str());
        }
    }

    tFirstCall = false;
}

bool MediaSourcePulseAudio::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    LOG(LOG_ERROR, "Wrong media type");
    return false;
}

bool MediaSourcePulseAudio::OpenAudioGrabDevice(int pSampleRate, int pChannels)
{
	pa_sample_spec 	tInputFormat;
	int				tRes;
    pa_usec_t 		tLatency;

    mMediaType = MEDIA_AUDIO;
    mOutputAudioChannels = pChannels;
    mOutputAudioSampleRate = pSampleRate;
    mOutputAudioFormat = AV_SAMPLE_FMT_S16; // assume we always want signed 16 bit
    mInputAudioChannels = pChannels;
    mInputAudioSampleRate = pSampleRate;
    mInputAudioFormat = AV_SAMPLE_FMT_S16; // we use signed 16 bit when opening the grabber device

    LOG(LOG_VERBOSE, "Trying to open the audio source");

    SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(PulseAudio)");

    if (mMediaSourceOpened)
        return false;

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
    {
        LOG(LOG_VERBOSE, "Using default audio device");
        mDesiredDevice = "";
    }

    tInputFormat.format = PA_SAMPLE_S16LE;
    tInputFormat.rate = pSampleRate;
    tInputFormat.channels = pChannels;

	// create a new recording stream
	if (!(mInputStream = pa_simple_new(NULL, "Homer-Conferencing", PA_STREAM_RECORD, (mDesiredDevice != "" ? mDesiredDevice.c_str() : NULL) /* dev Name */, GetStreamName().c_str(), &tInputFormat, NULL, NULL, &tRes)))
	{
	    LOG(LOG_ERROR, "Couldn't create PulseAudio stream because %s(%d)", pa_strerror(tRes), tRes);
	    return false;
	}

    if ((tLatency = pa_simple_get_latency(mInputStream, &tRes)) == (pa_usec_t) -1)
    {
        LOG(LOG_ERROR, "Couldn't determine the latency of the output stream because %s(%d)", pa_strerror(tRes), tRes);
        pa_simple_free(mInputStream);
        mInputStream = NULL;
        return false;
    }

    mCurrentDevice = mDesiredDevice;
    mInputFrameRate = (float)mOutputAudioSampleRate /* 44100 samples per second */ / MEDIA_SOURCE_SAMPLES_PER_BUFFER /* 1024 samples per frame */;
	mOutputFrameRate = mInputFrameRate;

    //######################################################
    //### give some verbose output
    //######################################################
    LOG(LOG_INFO, "%s-audio source opened...", "MediaSourcePortAudio");
    LOG(LOG_INFO,"    ..sample rate: %d", mOutputAudioSampleRate);
    LOG(LOG_INFO,"    ..channels: %d", mOutputAudioChannels);
    LOG(LOG_INFO,"    ..desired device: %s", mDesiredDevice.c_str());
    LOG(LOG_INFO,"    ..selected device: %s", mCurrentDevice.c_str());
    LOG(LOG_INFO,"    ..latency: %"PRIu64" seconds", (uint64_t)tLatency * 1000 * 1000);
    LOG(LOG_INFO,"    ..sample format: %d", PA_SAMPLE_S16LE);

    mFrameNumber = 0;
    mMediaType = MEDIA_AUDIO;
    mMediaSourceOpened = true;

    return true;
}

bool MediaSourcePulseAudio::CloseGrabDevice()
{
    bool 	tResult = false;
    int 	tRes;

    LOG(LOG_VERBOSE, "Going to close");

    if (mMediaType == MEDIA_VIDEO)
    {
        LOG(LOG_ERROR, "Wrong media type");
        return false;
    }

    if (mMediaSourceOpened)
    {
        StopRecording();
        StopGrabbing();

        mMediaSourceOpened = false;

        if (mInputStream != NULL)
        {
        	LOG(LOG_VERBOSE, "..draining stream");
        	if (pa_simple_drain(mInputStream, &tRes) < 0)
        	{
        	    LOG(LOG_ERROR, "Couldn't drain the output stream because %s(%d)", pa_strerror(tRes), tRes);
        	}
        	LOG(LOG_VERBOSE, "..closing stream");
        	pa_simple_free(mInputStream);
        }

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mGrabbingStopped = false;
    mMediaType = MEDIA_UNKNOWN;

    ResetPacketStatistic();

    return tResult;
}

int MediaSourcePulseAudio::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    int 	tResult;
    int		tRes;

    #ifdef MSPUA_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Going to grab new input data");
    #endif

    // lock grabbing
    mGrabMutex.lock();

    if (mGrabbingStopped)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        MarkGrabChunkFailed("Tried to grab while audio source is paused");

        return GRAB_RES_INVALID;
    }

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        MarkGrabChunkFailed("Tried to grab while audio source is closed");

        return GRAB_RES_INVALID;
    }

    if (pChunkSize != MEDIA_SOURCE_SAMPLES_BUFFER_SIZE)
    	pChunkSize = MEDIA_SOURCE_SAMPLES_BUFFER_SIZE;

	if (pa_simple_read(mInputStream, (void *)pChunkBuffer, (size_t)pChunkSize, &tRes) < 0)
	{
		LOG(LOG_ERROR, "Couldn't write audio chunk of %d bytes to output stream because %s(%d)", pChunkSize, pa_strerror(tRes), tRes);
	}

	//TODO: use pa_simple_get_latency() and determine the grabber latency -> but until now we do not support timestamps for A/V grabber

    // re-encode the frame and write it to file
    if ((mRecording) && (pChunkSize > 0))
        RecordSamples((int16_t *)pChunkBuffer, pChunkSize);

    // unlock grabbing
    mGrabMutex.unlock();

    // log statistics about raw PCM audio data stream
    AnnouncePacket(pChunkSize);

    mFrameNumber++;

    // acknowledge success
    MarkGrabChunkSuccessful(mFrameNumber);

    return mFrameNumber;
}

bool MediaSourcePulseAudio::SupportsRecording()
{
    return true;
}

string MediaSourcePulseAudio::GetCodecName()
{
    return "Raw";
}

string MediaSourcePulseAudio::GetCodecLongName()
{
    return "Raw";
}

}} //namespaces
