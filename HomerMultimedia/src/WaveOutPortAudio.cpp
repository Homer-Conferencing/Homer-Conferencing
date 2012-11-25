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
 * Purpose: Wave output based on PortAudio
 * Author:  Thomas Volkert
 * Since:   2012-05-07
 */

#include <Header_PortAudio.h>
#include <ProcessStatisticService.h>
#include <WaveOutPortAudio.h>
#include <MediaSourcePortAudio.h>
#include <Logger.h>

using namespace std;
using namespace Homer::Monitor;

namespace Homer { namespace Multimedia {

int WaveOutPortAudio::mOpenStreams = 0;

///////////////////////////////////////////////////////////////////////////////

// de/activate
//#define WOPA_AUTO_START_PLAYBACK

// deactivate OSS playback: leads to program stops
//#define WOPA_AVOID_OSS_PLAY

///////////////////////////////////////////////////////////////////////////////

WaveOutPortAudio::WaveOutPortAudio(string pOutputName, string pDesiredDevice):
    WaveOut(pOutputName)
{
    MediaSourcePortAudio::PortAudioInit();
    LOG(LOG_VERBOSE, "Creating wave out for %s on device %s", pOutputName.c_str(), pDesiredDevice.c_str());

    if ((pDesiredDevice != "") && (pDesiredDevice != "auto"))
    {
        bool tNewDeviceSelected = false;
        tNewDeviceSelected = SelectDevice(pDesiredDevice);
        if (!tNewDeviceSelected)
            LOG(LOG_INFO, "Haven't selected new PortAudio device when creating source object");
    }

    mWaitingForFirstBuffer = false;

    LOG(LOG_VERBOSE, "Created");
}

WaveOutPortAudio::~WaveOutPortAudio()
{
	LOG(LOG_VERBOSE, "Going to destroy PulseAudio wave out object..");
    if (mWaveOutOpened)
    {
    	LOG(LOG_VERBOSE, "..stopping wave out");
        Stop();
        LOG(LOG_VERBOSE, "..closing wave out device");
        CloseWaveOutDevice();
    }else
    	LOG(LOG_VERBOSE, "Skipped CloseWaveOutDevice() because device is already closed");

    LOG(LOG_VERBOSE, "Destroyed");
}

void WaveOutPortAudio::getAudioDevices(AudioDevices &pAList)
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

    tDevice.Name = "auto";
    tDevice.Card = "";
    tDevice.Desc = "automatic device selection";
    tDevice.IoType = "Input/Output";

    pAList.push_back(tDevice);

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

        // if device is able to play samples with 44.1 kHz add this device to the result list
        if ((tDevice.IoType.find("Output") != string::npos) && (tDeviceInfo->defaultSampleRate == 44100.0))
        {
            #ifdef WOPA_AVOID_OSS_PLAY
                if (string(Pa_GetHostApiInfo( tDeviceInfo->hostApi)->name) != "OSS")
            #endif
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
            LOG(LOG_VERBOSE, "..default sample rate: %8.0f Hz", tDeviceInfo->defaultSampleRate);
            LOG(LOG_VERBOSE, "..default low input latency  : %8.4f seconds", tDeviceInfo->defaultLowInputLatency);
            LOG(LOG_VERBOSE, "..default low output latency : %8.4f seconds", tDeviceInfo->defaultLowOutputLatency);
            LOG(LOG_VERBOSE, "..default high input latency : %8.4f seconds", tDeviceInfo->defaultHighInputLatency);
            LOG(LOG_VERBOSE, "..default high output latency: %8.4f seconds", tDeviceInfo->defaultHighOutputLatency);
        }
    }

    tFirstCall = false;
}

bool WaveOutPortAudio::OpenWaveOutDevice(int pSampleRate, int pOutputChannels)
{
    PaError           		tErr = paNoError;
    PaStreamParameters      tOutputParameters;

    LOG(LOG_VERBOSE, "Trying to open the wave out device");

    if (mWaveOutOpened)
        return false;

    mSampleRate = pSampleRate;
    mAudioChannels = pOutputChannels;

    LOG(LOG_VERBOSE, "Desired device is %s", mDesiredDevice.c_str());

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
    {
        LOG(LOG_VERBOSE, "Using default audio device");
        mDesiredDevice = char(Pa_GetDefaultOutputDevice() + 48);
    }

    int tDeviceId= mDesiredDevice[0] - 48;
    if (tDeviceId < 0)
    	return false;

    LOG(LOG_VERBOSE, "Will open port audio device %d", tDeviceId);

    tOutputParameters.device = tDeviceId;
    tOutputParameters.channelCount = pOutputChannels;
    tOutputParameters.sampleFormat = paInt16;
    tOutputParameters.suggestedLatency = Pa_GetDeviceInfo(tOutputParameters.device)->defaultLowOutputLatency;
    tOutputParameters.hostApiSpecificStreamInfo = NULL;

    LOG(LOG_VERBOSE, "Going to open stream..");
    LOG(LOG_VERBOSE, "..selected sample rate: %d", mSampleRate);
    MediaSourcePortAudio::PortAudioLockStreamInterface();
    LOG(LOG_VERBOSE, "Already open WaveOutPortAudio streams: %d", mOpenStreams);

    // limit wave out for OSX: facing some problems in case the audio device is opened two times, this may also conflict with MediaSourcePortAudio
    // HINT: use WaveOutSdl instead in OSX because PortAudio doesn't support more than 1 stream per device
    #ifdef APPLE
    	if (mOpenStreams > 0)
    	{
    		LOG(LOG_ERROR, "Couldn't open stream because maximum of 1 streams reached");
    		MediaSourcePortAudio::PortAudioUnlockStreamInterface();
    		return false;
    	}
	#endif
    if((tErr = Pa_OpenStream(&mStream, NULL /* input parameters */, &tOutputParameters, mSampleRate, MEDIA_SOURCE_SAMPLES_PER_BUFFER, paClipOff | paDitherOff, PlayAudioHandler /* callback */, this)) != paNoError)
    {
        LOG(LOG_ERROR, "Couldn't open stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
        MediaSourcePortAudio::PortAudioUnlockStreamInterface();
        return false;
    }
    mOpenStreams++;
    MediaSourcePortAudio::PortAudioUnlockStreamInterface();
    LOG(LOG_VERBOSE, "..stream opened");

    mCurrentDevice = mDesiredDevice;

    //######################################################
    //### give some verbose output
    //######################################################
    LOG(LOG_INFO, "PortAudio wave out opened...");
    LOG(LOG_INFO,"    ..sample rate: %d", mSampleRate);
    LOG(LOG_INFO,"    ..channels: %d", pOutputChannels);
    LOG(LOG_INFO,"    ..desired device: %s", mDesiredDevice.c_str());
    LOG(LOG_INFO,"    ..selected device: %s", mCurrentDevice.c_str());
    LOG(LOG_INFO,"    ..suggested latency: %f seconds", tOutputParameters.suggestedLatency);
    LOG(LOG_INFO,"    ..sample format: %d", paInt16);
    //LOG(LOG_INFO,"    ..sample buffer size: %d", mSampleBufferSize);
    LOG(LOG_INFO, "Fifo opened...");
    if (mSampleFifo != NULL)
    	LOG(LOG_INFO, "    ..fill size: %d bytes", av_fifo_size(mSampleFifo));
    else
    	LOG(LOG_WARN, "    ..fill size: invalid");

    mWaveOutOpened = true;

    return true;
}

bool WaveOutPortAudio::CloseWaveOutDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close..");

    if (mWaveOutOpened)
    {
        StopFilePlayback();
        Stop();

        PaError tErr = paNoError;
        MediaSourcePortAudio::PortAudioLockStreamInterface();
		LOG(LOG_VERBOSE, "..closing stream");
        if ((tErr = Pa_CloseStream(mStream)) != paNoError)
        {
            LOG(LOG_ERROR, "Couldn't close stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
        }
        mOpenStreams--;
        MediaSourcePortAudio::PortAudioUnlockStreamInterface();

        LOG(LOG_INFO, "...closed");

        mWaveOutOpened = false;
        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

     ResetPacketStatistic();

    return tResult;
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
int WaveOutPortAudio::PlayAudioHandler(const void *pInputBuffer, void *pOutputBuffer, unsigned long pOutputSize, const PaStreamCallbackTimeInfo* pTimeInfo, unsigned long pStatus, void *pUserData)
{
    WaveOutPortAudio *tWaveOutPortAudio = (WaveOutPortAudio*)pUserData;
    int tOutputBufferMaxSize = (int)pOutputSize * 2 /* 16 bit LittleEndian */ * (tWaveOutPortAudio->mAudioChannels);

    #ifdef WOPA_DEBUG_HANDLER
		LOGEX(WaveOutPortAudio, LOG_WARN, "PlayAudioHandler CALLED");
	#endif

    tWaveOutPortAudio->AssignThreadName();

    if (tWaveOutPortAudio->mPlaybackStopped)
    {
		#ifdef WOPA_DEBUG_HANDLER
			LOGEX(WaveOutPortAudio, LOG_WARN, "PlayAudioHandler-start: RETURN WITH STREAM COMPLETE");
		#endif
        return paComplete;
    }
    int tUsedFifo = tWaveOutPortAudio->mPlaybackFifo->GetUsage();

    #ifdef WOPA_DEBUG_PACKETS
        LOGEX(WaveOutPortAudio, LOG_WARN, "Playing %d audio samples, time stamp of first sample: %f, current time stamp: %f", pOutputSize, pTimeInfo->outputBufferDacTime, pTimeInfo->currentTime);
        LOGEX(WaveOutPortAudio, LOG_WARN, "Audio FIFO has %d available buffers, entire FIFO size is %d", tUsedFifo, tWaveOutPortAudio->mPlaybackFifo->GetSize());
    #endif

    // should we drop the entire FIFO because we have a buffer overrun?
    if (tUsedFifo >= tWaveOutPortAudio->mPlaybackFifo->GetSize() - 1)
    {
        LOGEX(WaveOutPortAudio, LOG_WARN, "Audio FIFO buffer full, drop all audio chunks and restart");
        tWaveOutPortAudio->mPlaybackFifo->ClearFifo();
    }

    // should be complain about buffer underrun?
    if ((tUsedFifo == 0) && (!tWaveOutPortAudio->mWaitingForFirstBuffer))
    {
        tWaveOutPortAudio->mPlaybackGaps++;
        #ifdef WOPA_DEBUG_GAPS
            LOGEX(WaveOutPortAudio, LOG_WARN, "Audio FIFO empty, playback for %s is non continuous, found gaps: %ld", tWaveOutPortAudio->GetStreamName().c_str(), tWaveOutPortAudio->mPlaybackGaps);
        #endif
        memset(pOutputBuffer, 0, (size_t)tOutputBufferMaxSize);
        return paContinue;
    }else
    {
        #ifdef WOPA_DEBUG_HANDLER
            LOGEX(WaveOutPortAudio, LOG_VERBOSE, "Audio buffers in playback FIFO: %d", tUsedFifo);
        #endif
    }

    int tBufferSize;
    do {
        tBufferSize = tOutputBufferMaxSize;
		#ifdef WOPA_DEBUG_HANDLER
			LOGEX(WaveOutPortAudio, LOG_WARN, "PlayAudioHandler: reading the FIFO");
		#endif
        tWaveOutPortAudio->mPlaybackFifo->ReadFifo((char*)pOutputBuffer, tBufferSize);
		#ifdef WOPA_DEBUG_HANDLER
			LOGEX(WaveOutPortAudio, LOG_WARN, "PlayAudioHandler: got %d bytes from FIFO", tBufferSize);
		#endif
        if (tWaveOutPortAudio->mPlaybackStopped)
        {
    		#ifdef WOPA_DEBUG_HANDLER
    			LOGEX(WaveOutPortAudio, LOG_WARN, "PlayAudioHandler-end: RETURN WITH STREAM COMPLETE");
    		#endif
            return paComplete;
        }
        if (tWaveOutPortAudio->mWaitingForFirstBuffer)
        {
            tWaveOutPortAudio->mWaitingForFirstBuffer = false;
            #ifdef WOPA_DEBUG_HANDLER
                LOGEX(WaveOutPortAudio, LOG_WARN, "Got first audio buffer for playback");
            #endif
        }

        // return with a "complete" if an empty fragment was read from FIFO
        if (tBufferSize == 0)
        {
			#ifdef WOPA_DEBUG_HANDLER
				LOGEX(WaveOutPortAudio, LOG_WARN, "PlayAudioHandler-end: RETURN WITH STREAM COMPLETE");
			#endif
            return paComplete;
        }

		#ifdef WOPA_DEBUG_HANDLER
        	LOGEX(WaveOutPortAudio, LOG_WARN, "Got sample buffer of %d bytes for playback", tBufferSize);
		#endif
        if ((tBufferSize != tOutputBufferMaxSize) && (tBufferSize != 0))
            LOGEX(WaveOutPortAudio, LOG_WARN, "Audio buffer has wrong size of %d bytes, %d bytes needed", tBufferSize, tOutputBufferMaxSize);

    }while (tBufferSize != tOutputBufferMaxSize);

	#ifdef WOPA_DEBUG_HANDLER
		LOGEX(WaveOutPortAudio, LOG_WARN, "PlayAudioHandler RETURN WITH STREAM CONTINUE");
	#endif

    return paContinue;
}

void WaveOutPortAudio::AssignThreadName()
{
    if (mHaveToAssignThreadName)
    {
        if (mFilePlaybackLoops)
            SVC_PROCESS_STATISTIC.AssignThreadName("WaveOutPortAudio-File");
        else
            SVC_PROCESS_STATISTIC.AssignThreadName("WaveOutPortAudio-Mem");
        mHaveToAssignThreadName = false;
    }
}

bool WaveOutPortAudio::Play()
{
    PaError           tErr = paNoError;

    LOG(LOG_VERBOSE, "Starting playback stream..");

    mPlayMutex.lock();

    if (!mWaveOutOpened)
    {
        // unlock grabbing
        mPlayMutex.unlock();

        LOG(LOG_VERBOSE, "Playback device wasn't opened yet");

        return true;
    }

    if (!mPlaybackStopped)
    {
        // unlock grabbing
        mPlayMutex.unlock();

        LOG(LOG_VERBOSE, "Playback was already started");

        return true;
    }

    WaveOut::Play();

    // do we already play?
	MediaSourcePortAudio::PortAudioLockStreamInterface();
	if (Pa_IsStreamActive(mStream) == 0)
    {
        LOG(LOG_VERBOSE, "..going to start stream..");
        mWaitingForFirstBuffer = true;
        if((tErr = Pa_StartStream(mStream)) != paNoError)
        {
            if (tErr == paStreamIsNotStopped)
            {
                LOG(LOG_VERBOSE, "Couldn't start stream because it wasn't stopped", Pa_GetErrorText(tErr), tErr);
                if ((tErr = Pa_StopStream(mStream)) != paNoError)
                {
                    LOG(LOG_ERROR, "Couldn't stop stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
                }else
                {
                    tErr = Pa_StartStream(mStream);
                }
            }

            if (tErr != paNoError)
            {
                // unlock grabbing
                mPlayMutex.unlock();

                LOG(LOG_ERROR, "Couldn't start stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
                MediaSourcePortAudio::PortAudioUnlockStreamInterface();
                return false;
            }
        }

        // wait until the port audio stream becomes active
        int tLoop = 0;
        while((Pa_IsStreamActive(mStream)) == 0)
        {
        	if (tLoop > 10)
        	{
                // unlock grabbing
                mPlayMutex.unlock();

                LOG(LOG_WARN, "Was not able to start playback stream");
        		MediaSourcePortAudio::PortAudioUnlockStreamInterface();
        	    return false;
        	}
        	LOG(LOG_VERBOSE, "..wait for stream start, loop count: %d", tLoop);
        	tLoop++;
            // wait some time
            Thread::Suspend(50 * 1000);
        }
        LOG(LOG_VERBOSE, "..stream was started");
    }else
    	LOG(LOG_VERBOSE, "Stream was already started");
	MediaSourcePortAudio::PortAudioUnlockStreamInterface();

    // unlock grabbing
    mPlayMutex.unlock();

    return true;
}

void WaveOutPortAudio::Stop()
{
    PaError tErr = paNoError;

    LOG(LOG_VERBOSE, "Stopping playback stream..");

    mPlayMutex.lock();

    if (!mWaveOutOpened)
    {
        // unlock grabbing
        mPlayMutex.unlock();

        LOG(LOG_VERBOSE, "Playback device wasn't opened yet");

        return;
    }

    if (mPlaybackStopped)
    {
        // unlock grabbing
        mPlayMutex.unlock();

        LOG(LOG_VERBOSE, "Playback was already stopped");

        return;
    }

    WaveOut::Stop();
    mFilePlaybackLoops = 0;

    // make sure no one waits for audio anymore -> send an empty buffer to FIFO and force a return from a possible ReadFifo() call
    LOG(LOG_VERBOSE, "..writing an empty packet to FIFO to force wake up");
    char tData[4];
    mPlaybackFifo->WriteFifo(tData, 0);

    LOG(LOG_VERBOSE, "..empty packets to FIFO were written to force a wake up");

    MediaSourcePortAudio::PortAudioLockStreamInterface();
    if (Pa_IsStreamActive(mStream) == 1)
    {
        LOG(LOG_VERBOSE, "..going to stop stream..");

        //HINT: for OSX we use a patched portaudio version which doesn't call waitUntilBlioWriteBufferIsFlushed and therefore ignores pending audio buffers but doesn't hang here anymore
        if ((tErr = Pa_StopStream(mStream)) != paNoError)
        {
            // unlock grabbing
            mPlayMutex.unlock();

            LOG(LOG_ERROR, "Couldn't stop stream because \"%s\"", Pa_GetErrorText(tErr));
    		MediaSourcePortAudio::PortAudioUnlockStreamInterface();
            return;
        }

        LOG(LOG_VERBOSE, "..clearing FIFO buffer");
        mPlaybackFifo->ClearFifo();

        // wait until the port audio stream becomes inactive
        int tLoop = 0;
        while((Pa_IsStreamActive(mStream)) == 1)
        {
        	if (tLoop > 10)
        	{
                // unlock grabbing
                mPlayMutex.unlock();

                LOG(LOG_WARN, "Was not able to stop playback stream");
        		MediaSourcePortAudio::PortAudioUnlockStreamInterface();
        	    return;
        	}
        	LOG(LOG_VERBOSE, "..wait for stream stop, loop count: %d", tLoop);
        	tLoop++;
            // wait some time
            Thread::Suspend(50 * 1000);
        }
        LOG(LOG_VERBOSE, "..stream was stopped");
    }else
    	LOG(LOG_VERBOSE, "Stream was already stopped");
	MediaSourcePortAudio::PortAudioUnlockStreamInterface();

    // unlock grabbing
    mPlayMutex.unlock();
}

}} //namespaces
