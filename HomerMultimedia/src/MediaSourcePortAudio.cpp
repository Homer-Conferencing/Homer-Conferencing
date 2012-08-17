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
 * Purpose: PortAudio capture implementation
 * Author:  Thomas Volkert
 * Since:   2012-04-25
 */

#include <Header_PortAudio.h>
#include <MediaFifo.h>
#include <MediaSourcePortAudio.h>
#include <ProcessStatisticService.h>
#include <Logger.h>
#include <HBThread.h>

#include <string.h>
#include <stdlib.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

Mutex MediaSourcePortAudio::sPaInitMutex;
bool MediaSourcePortAudio::sPaInitiated = false;
Mutex MediaSourcePortAudio::sPaStreamMutex;

void MediaSourcePortAudio::PortAudioInit()
{
    sPaInitMutex.lock();
    if (!sPaInitiated)
    {
        // initialize portaudio library
        LOGEX(MediaSourcePortAudio, LOG_VERBOSE, "Initiated portaudio with result: %d", Pa_Initialize());
        sPaInitiated = true;
    }
    sPaInitMutex.unlock();
}

void MediaSourcePortAudio::PortAudioLockStreamInterface()
{
	LOGEX(MediaSourcePortAudio, LOG_VERBOSE, "Locking portaudio stream interface");
	sPaStreamMutex.lock();
	LOGEX(MediaSourcePortAudio, LOG_VERBOSE, "Stream interface locked");
}

void MediaSourcePortAudio::PortAudioUnlockStreamInterface()
{
	LOGEX(MediaSourcePortAudio, LOG_VERBOSE, "Unlocking portaudio stream interface");
	sPaStreamMutex.unlock();
	LOGEX(MediaSourcePortAudio, LOG_VERBOSE, "Stream interface unlocked");
}

MediaSourcePortAudio::MediaSourcePortAudio(string pDesiredDevice):
    MediaSource("PortAudio: local capture")
{
    mSourceType = SOURCE_DEVICE;
    ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);
    mCaptureFifo = new MediaFifo(MEDIA_SOURCE_SAMPLES_CAPTURE_FIFO_SIZE, MEDIA_SOURCE_SAMPLES_BUFFER_SIZE, "MediaSourcePortAudio");

    PortAudioInit();
    if (pDesiredDevice != "")
    {
        bool tNewDeviceSelected = false;
        SelectDevice(pDesiredDevice, MEDIA_AUDIO, tNewDeviceSelected);
        if (!tNewDeviceSelected)
            LOG(LOG_INFO, "Haven't selected new PortAudio device when creating source object");
    }

    LOG(LOG_VERBOSE, "Created");
}

MediaSourcePortAudio::~MediaSourcePortAudio()
{
    LOG(LOG_VERBOSE, "Destroying PortAudio grabber");

    StopGrabbing();

    if (mMediaSourceOpened)
        CloseGrabDevice();

    delete mCaptureFifo;

    LOG(LOG_VERBOSE, "Destroyed");
}

void MediaSourcePortAudio::getAudioDevices(AudioDevices &pAList)
{
    static bool tFirstCall = true;
    AudioDeviceDescriptor tDevice;
    PaStreamParameters  inputParameters, outputParameters;

    #ifdef MSCA_DEBUG_PACKETS
        tFirstCall = true;
    #endif

    int tDevCount = Pa_GetDeviceCount();
    if (tFirstCall)
    {
        LOG(LOG_VERBOSE, "Enumerating hardware..");
        LOG(LOG_VERBOSE, "PortAudio version \"%s\"(%d)", Pa_GetVersionText(), Pa_GetVersion());
        LOG(LOG_VERBOSE, "Detected %d audio devices", tDevCount);
    }

    for (int i = 0; i < tDevCount; i++)
    {
        const PaDeviceInfo *tDeviceInfo = Pa_GetDeviceInfo(i);
        tDevice.Name = string(tDeviceInfo->name);
        tDevice.Card = char(i + 48);
        if(tDeviceInfo->maxInputChannels)
            tDevice.IoType = "Input";
        if(tDeviceInfo->maxOutputChannels)
            tDevice.IoType = "Output";
        if((tDeviceInfo->maxInputChannels) && (tDeviceInfo->maxOutputChannels))
            tDevice.IoType = "Input/Outputs";
        tDevice.Desc = string(Pa_GetHostApiInfo( tDeviceInfo->hostApi)->name) + " based audio device";

        // if device is able to capture samples with 44.1 kHz add this device to the result list
        if ((tDevice.IoType.find("Input") != string::npos)  && (tDeviceInfo->defaultSampleRate == 44100.0))
        {
            tDevice.Type = Microphone;
            pAList.push_back(tDevice);
        }

        if (tFirstCall)
        {
            LOG(LOG_VERBOSE, "Device %d.. %s", i, (tDeviceInfo->defaultSampleRate == 44100.0) ? " " : "[unsupported, sample rate must be 44.1 kHz]");

            // mark global and API specific default devices
            if (i == Pa_GetDefaultInputDevice())
            {
                LOG(LOG_VERBOSE, "..is DEFAULT INPUT device");
            } else
            {
                if (i == Pa_GetHostApiInfo(tDeviceInfo->hostApi)->defaultInputDevice)
                {
                    const PaHostApiInfo *tHostApiInfo = Pa_GetHostApiInfo(tDeviceInfo->hostApi);
                    LOG(LOG_VERBOSE, "..default %s input", tHostApiInfo->name );
                }
            }

            if (i == Pa_GetDefaultOutputDevice())
            {
                LOG(LOG_VERBOSE, "..is DEFAULT OUTPUT device");
            } else
            {
                if (i == Pa_GetHostApiInfo(tDeviceInfo->hostApi)->defaultOutputDevice)
                {
                    const PaHostApiInfo *tHostApiInfo = Pa_GetHostApiInfo(tDeviceInfo->hostApi);
                    LOG(LOG_VERBOSE, "..default %s output", tHostApiInfo->name );
                }
            }

            // print device info fields
            LOG(LOG_VERBOSE, "..name: \"%s\"", tDeviceInfo->name);
            LOG(LOG_VERBOSE, "..host API: \"%s\"", Pa_GetHostApiInfo( tDeviceInfo->hostApi)->name);
            LOG(LOG_VERBOSE, "..max. inputs channels: %d", tDeviceInfo->maxInputChannels);
            LOG(LOG_VERBOSE, "..max. outputs channels: %d", tDeviceInfo->maxOutputChannels);
            LOG(LOG_VERBOSE, "..default low input latency  : %8.4f seconds", tDeviceInfo->defaultLowInputLatency);
            LOG(LOG_VERBOSE, "..default low output latency : %8.4f seconds", tDeviceInfo->defaultLowOutputLatency);
            LOG(LOG_VERBOSE, "..default high input latency : %8.4f seconds", tDeviceInfo->defaultHighInputLatency);
            LOG(LOG_VERBOSE, "..default high output latency: %8.4f seconds", tDeviceInfo->defaultHighOutputLatency);
            LOG(LOG_VERBOSE, "..default sample rate: %8.0f Hz", tDeviceInfo->defaultSampleRate);
        }
    }

    tFirstCall = false;
}

/* audio callback function

 @param input and @param output are either arrays of interleaved samples or;
 if non-interleaved samples were requested using the paNonInterleaved sample
 format flag, an array of buffer pointers, one non-interleaved buffer for
 each channel.

 The format, packing and number of channels used by the buffers are
 determined by parameters to Pa_OpenStream().

 @param frameCount The number of sample frames to be processed by
 the stream callback.

 @param timeInfo The time in seconds when the first sample of the input
 buffer was received at the audio input, the time in seconds when the first
 sample of the output buffer will begin being played at the audio output, and
 the time in seconds when the stream callback was called.
 See also Pa_GetStreamTime()

 @param statusFlags Flags indicating whether input and/or output buffers
 have been inserted or will be dropped to overcome underflow or overflow
 conditions.

 @param userData The value of a user supplied pointer passed to
 Pa_OpenStream() intended for storing synthesis data etc.

 @return
 The stream callback should return one of the values in the
 PaStreamCallbackResult enumeration. To ensure that the callback continues
 to be called, it should return paContinue (0). Either paComplete or paAbort
 can be returned to finish stream processing, after either of these values is
 returned the callback will not be called again. If paAbort is returned the
 stream will finish as soon as possible. If paComplete is returned, the stream
 will continue until all buffers generated by the callback have been played.
 This may be useful in applications such as soundfile players where a specific
 duration of output is required. However, it is not necessary to utilize this
 mechanism as Pa_StopStream(), Pa_AbortStream() or Pa_CloseStream() can also
 be used to stop the stream. The callback must always fill the entire output
 buffer irrespective of its return value.

 */
int MediaSourcePortAudio::RecordedAudioHandler(const void *pInputBuffer, void *pOutputBuffer, unsigned long pInputSize, const PaStreamCallbackTimeInfo* pTimeInfo, PaStreamCallbackFlags pStatus, void *pUserData)
{
    MediaSourcePortAudio *tMediaSourcePortAudio = (MediaSourcePortAudio*)pUserData;

    tMediaSourcePortAudio->AssignThreadName();

    if (tMediaSourcePortAudio->mGrabbingStopped)
        return paComplete;

    #ifdef MSPA_DEBUG_HANDLER
        LOGEX(MediaSourcePortAudio, LOG_VERBOSE, "Captured %d audio samples, time stamp of first sample: %f, current time stamp: %f", pInputSize, pTimeInfo->inputBufferAdcTime, pTimeInfo->currentTime);
    #endif

    tMediaSourcePortAudio->mCaptureFifo->WriteFifo((char*)pInputBuffer, (int)pInputSize * 2 /* 16 bit LittleEndian */ * (tMediaSourcePortAudio->mStereo ? 2 : 1));

    return paContinue;
}

void MediaSourcePortAudio::AssignThreadName()
{
    if (mHaveToAssignThreadName)
    {
        SVC_PROCESS_STATISTIC.AssignThreadName("PortAudio-Capture");
        mHaveToAssignThreadName = false;
    }
}

bool MediaSourcePortAudio::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    LOG(LOG_ERROR, "Wrong media type");
    return false;
}

bool MediaSourcePortAudio::OpenAudioGrabDevice(int pSampleRate, bool pStereo)
{
    unsigned int tChannels = pStereo?2:1;
    PaError           tErr = paNoError;
    PaStreamParameters      tInputParameters;

    LOG(LOG_VERBOSE, "Trying to open the audio source");

    if (mMediaType == MEDIA_VIDEO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return false;
    }

    SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(PortAudio)");

    if (mMediaSourceOpened)
        return false;

    mSampleRate = pSampleRate;
    mStereo = pStereo;

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
    {
        LOG(LOG_VERBOSE, "Using default audio device");
        mDesiredDevice = char(Pa_GetDefaultInputDevice() + 48);
    }

    int tDeviceId= mDesiredDevice[0] - 48;
    LOG(LOG_VERBOSE, "Will open port audio device %d", tDeviceId);

    if (tDeviceId < 0)
    {
    	LOG(LOG_ERROR, "Selected audio device id %d is invalid", tDeviceId);
    	return false;
    }
    tInputParameters.device = tDeviceId;
    tInputParameters.channelCount = tChannels;
    tInputParameters.sampleFormat = paInt16;
    const PaDeviceInfo *tDevInfo = Pa_GetDeviceInfo(tInputParameters.device);
    if (tDevInfo != NULL)
    	tInputParameters.suggestedLatency = tDevInfo->defaultLowInputLatency;
    else
    	tInputParameters.suggestedLatency = 0.100;
    tInputParameters.hostApiSpecificStreamInfo = NULL;

    LOG(LOG_VERBOSE, "Going to open stream..");
    LOG(LOG_VERBOSE, "..selected sample rate: %d", mSampleRate);
    mCaptureDuplicateMonoStream = false;
    PortAudioLockStreamInterface();
    if((tErr = Pa_OpenStream(&mStream, &tInputParameters, NULL /* output parameters */, mSampleRate, MEDIA_SOURCE_SAMPLES_PER_BUFFER, paClipOff | paDitherOff, RecordedAudioHandler, this)) != paNoError)
    {
    	if ((pStereo) && (tErr == paInvalidChannelCount))
    	{
    		LOG(LOG_WARN, "Got channel count problem when stereo mode is selected, will try mono mode instead");
    	    tInputParameters.channelCount = 1;
    	    tChannels = 1;
    	    if((tErr = Pa_OpenStream(&mStream, &tInputParameters, NULL /* output parameters */, mSampleRate, MEDIA_SOURCE_SAMPLES_PER_BUFFER, paClipOff | paDitherOff, RecordedAudioHandler, this)) != paNoError)
    	    {
        		LOG(LOG_ERROR, "Couldn't open stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
        		PortAudioUnlockStreamInterface();
        		return false;
    	    }else
    	    {
    	    	mCaptureDuplicateMonoStream = true;
    	    	mStereo = false;
    	    }
    	}else
    	{
    		LOG(LOG_ERROR, "Couldn't open stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
    		PortAudioUnlockStreamInterface();
    		return false;
    	}
    }
    LOG(LOG_VERBOSE, "..stream opened");

    LOG(LOG_VERBOSE, "Going to start stream..");
    if((tErr = Pa_StartStream(mStream)) != paNoError)
    {
        LOG(LOG_ERROR, "Couldn't start stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
        PortAudioUnlockStreamInterface();
        return false;
    }
    PortAudioUnlockStreamInterface();

    mCurrentDevice = mDesiredDevice;

    //######################################################
    //### give some verbose output
    //######################################################
    LOG(LOG_INFO, "%s-audio source opened...", "MediaSourcePortAudio");
    LOG(LOG_INFO,"    ..sample rate: %d", mSampleRate);
    LOG(LOG_INFO,"    ..channels: %d", tChannels);
    LOG(LOG_INFO,"    ..desired device: %s", mDesiredDevice.c_str());
    LOG(LOG_INFO,"    ..selected device: %s", mCurrentDevice.c_str());
    LOG(LOG_INFO,"    ..suggested latency: %f seconds", tInputParameters.suggestedLatency);
    LOG(LOG_INFO,"    ..sample format: %d", paInt16);
    //LOG(LOG_INFO,"    ..sample buffer size: %d", mSampleBufferSize);

    mChunkNumber = 0;
    mMediaType = MEDIA_AUDIO;
    mMediaSourceOpened = true;
    mHaveToAssignThreadName = true;

    return true;
}

bool MediaSourcePortAudio::CloseGrabDevice()
{
    bool tResult = false;

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

        // wait until the port audio stream becomes inactive
        while((Pa_IsStreamActive(mStream)) == 1)
        {
            // wait some time
            Thread::Suspend(250 * 1000);
        }

        PaError tErr = paNoError;
        PortAudioLockStreamInterface();
        LOG(LOG_VERBOSE, "..closing PulseAudio stream");
        if ((tErr = Pa_CloseStream(mStream)) != paNoError)
        {
            LOG(LOG_ERROR, "Couldn't close stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
        }else
            LOG(LOG_VERBOSE, "..PulseAudio stream closed");
        PortAudioUnlockStreamInterface();

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mGrabbingStopped = false;
    mMediaType = MEDIA_UNKNOWN;

    ResetPacketStatistic();

    return tResult;
}

int MediaSourcePortAudio::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    int tResult;

    #ifdef MSPA_DEBUG_PACKETS
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

    #ifdef MSPA_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Waiting for a new chunk of max. %d captured bytes", pChunkSize);
    #endif

    mCaptureFifo->ReadFifo((char*)pChunkBuffer, pChunkSize);

    if (mCaptureDuplicateMonoStream)
    {
    	// assume buffer of 16 bit signed integer samples
		short int *tBuffer = (short int*)pChunkBuffer;
		// assume 16 bits per sample
		int tSampleCount = pChunkSize / 2;
		#ifdef MSPA_DEBUG_PACKETS
			LOG(LOG_VERBOSE, "Duplicating %d samples from mono to stereo", tSampleCount);
		#endif
		// duplicate each sample: mono ==> stereo
		for (int i = tSampleCount - 1; i > 0; i--)
    	{
			tBuffer[i * 2 + 1] = tBuffer[i];
			tBuffer[i * 2] = tBuffer[i];
    	}
    	pChunkSize *= 2;
    }
    #ifdef MSPA_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Delivering audio chunk of %d bytes", pChunkSize);
    #endif

    // re-encode the frame and write it to file
    if ((mRecording) && (pChunkSize > 0))
        RecordSamples((int16_t *)pChunkBuffer, pChunkSize);

    // unlock grabbing
    mGrabMutex.unlock();

    // log statistics about raw PCM audio data stream
    AnnouncePacket(pChunkSize);

    mChunkNumber++;

    // acknowledge success
    MarkGrabChunkSuccessful(mChunkNumber);

    return mChunkNumber;
}

bool MediaSourcePortAudio::SupportsRecording()
{
    return true;
}

void MediaSourcePortAudio::StopGrabbing()
{
    LOG(LOG_VERBOSE, "Stopping PortAudio grabbing..");

    LOG(LOG_VERBOSE, "..mark as stopped");
    MediaSource::StopGrabbing();

    if (mMediaSourceOpened)
    {
        if (Pa_IsStreamActive(mStream) == 1)
        {
            PaError tErr = paNoError;
            PortAudioLockStreamInterface();
            LOG(LOG_VERBOSE, "..abort PortAudio stream");
            if ((tErr = Pa_AbortStream(mStream)) != paNoError)
                LOG(LOG_ERROR, "Couldn't abort stream because \"%s\"", Pa_GetErrorText(tErr));

            // wait until the port audio stream becomes inactive
            while((Pa_IsStreamActive(mStream)) == 1)
            {
                // wait some time
                LOG(LOG_VERBOSE, "..waiting for deactivated PortAudio stream");
                Thread::Suspend(250 * 1000);
            }
            PortAudioUnlockStreamInterface();
        }

        // make sure no one waits for audio anymore -> send an empty buffer to FIFO and force a return from a possible ReadFifo() call
        char tData[4];
        mCaptureFifo->WriteFifo(tData, 0);
    }
}

string MediaSourcePortAudio::GetCodecName()
{
    return "Raw";
}

string MediaSourcePortAudio::GetCodecLongName()
{
    return "Raw";
}

}} //namespaces
