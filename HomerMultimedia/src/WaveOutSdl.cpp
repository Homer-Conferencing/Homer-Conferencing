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
 * Purpose: Wave output based on SDL
 * Author:  Thomas Volkert
 * Since:   2012-05-13
 */

#include <ProcessStatisticService.h>
#include <AudioOutSdl.h>
#include <WaveOutSdl.h>
#include <Logger.h>
#include <Header_PortAudio.h>

using namespace std;
using namespace Homer::Monitor;
using namespace Homer::SoundOutput;

namespace Homer { namespace Multimedia {

Mutex WaveOutSdl::mSdlInitMutex;
bool WaveOutSdl::mSdlInitiated = false;

void WaveOutSdl::SdlAudioInit(string pDeviceName)
{
	mSdlInitMutex.lock();
    if (!mSdlInitiated)
    {
        // initialize SDL library
    	LOGEX(WaveOutSdl, LOG_WARN, "Will use SDL audio playback on device %s", pDeviceName.c_str());
    	AUDIOOUTSDL.OpenPlaybackDevice(44100, true, "", pDeviceName);
    	//HINT: we neglect     AUDIOOUTSDL.ClosePlaybackDevice();

        LOGEX(WaveOutSdl, LOG_VERBOSE, "Initiated SDL with result: %d", Pa_Initialize());
        mSdlInitiated = true;
    }
    mSdlInitMutex.unlock();
}

WaveOutSdl::WaveOutSdl(string pDesiredDevice):
    WaveOut("SDL-Playback")
{
    if ((pDesiredDevice != "") && (pDesiredDevice != "auto"))
    {
        bool tNewDeviceSelected = false;
        tNewDeviceSelected = SelectDevice(pDesiredDevice);
        if (!tNewDeviceSelected)
            LOG(LOG_INFO, "Haven't selected new SDL device when creating source object");
    }

	SdlAudioInit(pDesiredDevice);

    LOG(LOG_VERBOSE, "Created");
}

WaveOutSdl::~WaveOutSdl()
{
    if (mWaveOutOpened)
    {
        Stop();
        CloseWaveOutDevice();
    }

    LOG(LOG_VERBOSE, "Destroyed");
}

void WaveOutSdl::getAudioDevices(AudioDevicesList &pAList) // use PortAudio to enumerate audio devices for SDL based playback
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
            pAList.push_back(tDevice);

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

bool WaveOutSdl::OpenWaveOutDevice(int pSampleRate, bool pStereo)
{
    mAudioChannel = AUDIOOUTSDL.AllocateChannel();

    //######################################################
    //### give some verbose output
    //######################################################
    LOG(LOG_INFO, "SDL wave out opened...");
    LOG(LOG_INFO,"    ..sample rate: 44100 Hz", mSampleRate);
    LOG(LOG_INFO,"    ..channels: 2");
    LOG(LOG_INFO,"    ..desired device: %s", mDesiredDevice.c_str());
    LOG(LOG_INFO,"    ..selected device: %s", mCurrentDevice.c_str());

    mWaveOutOpened = true;

    Play();

    return true;
}

bool WaveOutSdl::CloseWaveOutDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close");

    if (mWaveOutOpened)
    {
        // terminate possibly running main loop for file based playback
        mFilePlaybackNeeded = false;
        mFilePlaybackCondition.SignalAll();
        StopThread(3000);

        Stop();

        AUDIOOUTSDL.ReleaseChannel(mAudioChannel);

        LOG(LOG_INFO, "...closed");

        mWaveOutOpened = false;
        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

     ResetPacketStatistic();

    return tResult;
}

void WaveOutSdl::AssignThreadName()
{
    if (mHaveToAssignThreadName)
    {
        SVC_PROCESS_STATISTIC.AssignThreadName("WaveOutSdl-File");
        mHaveToAssignThreadName = false;
    }
}

bool WaveOutSdl::WriteChunk(void* pChunkBuffer, int pChunkSize)
{
    mPlayMutex.lock();

    if (!mWaveOutOpened)
    {
        // unlock grabbing
        mPlayMutex.unlock();

        LOG(LOG_ERROR, "Tried to play while WaveOut device is closed");

        return false;
    }

    if (mPlaybackStopped)
    {
        // unlock grabbing
        mPlayMutex.unlock();

        LOG(LOG_VERBOSE, "Will automatically start the audio stream");

		Play();

		mPlayMutex.lock();
    }

    if (mVolume != 100)
    {
        //LOG(LOG_WARN, "Got %d bytes and will adapt volume", pChunkSize);
        short int *tSamples = (short int*)pChunkBuffer;
        for (int i = 0; i < pChunkSize / 2; i++)
        {
            int tNewSample = (int)tSamples[i] * mVolume / 100;
            if (tNewSample < -32767)
                tNewSample = -32767;
            if (tNewSample >  32767)
                tNewSample =  32767;
            //LOG(LOG_WARN, "Entry %d from %d to %d", i, (int)tSamples[i], (int)tNewSample);
            tSamples[i] = (short int)tNewSample;
        }
    }
    #ifdef WOS_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Got %d samples for audio output stream", pChunkSize / 4);
    #endif

    if (pChunkSize / 4 != MEDIA_SOURCE_SAMPLES_PER_BUFFER)
    {
        #ifdef WOS_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Will use FIFO because input chunk has wrong size, need %d samples per chunk buffer", MEDIA_SOURCE_SAMPLES_PER_BUFFER);
        #endif
        if (av_fifo_realloc2(mSampleFifo, av_fifo_size(mSampleFifo) + pChunkSize) < 0)
        {
            LOG(LOG_ERROR, "Reallocation of FIFO audio buffer failed");
            return false;
        }

        // write new samples into fifo buffer
        av_fifo_generic_write(mSampleFifo, pChunkBuffer, pChunkSize, NULL);

        char tAudioBuffer[MEDIA_SOURCE_SAMPLES_BUFFER_SIZE];
        int tAudioBufferSize;
        while (av_fifo_size(mSampleFifo) >=MEDIA_SOURCE_SAMPLES_BUFFER_SIZE)
        {
            tAudioBufferSize = MEDIA_SOURCE_SAMPLES_BUFFER_SIZE;

            // read sample data from the fifo buffer
            HM_av_fifo_generic_read(mSampleFifo, (void*)tAudioBuffer, tAudioBufferSize);

            #ifdef WOPA_DEBUG_PACKETS
                LOG(LOG_VERBOSE, "Writing %d samples to audio output stream", tAudioBufferSize / 4);
            #endif

            AUDIOOUTSDL.Enqueue(mAudioChannel, (void*)tAudioBuffer, tAudioBufferSize);
            AUDIOOUTSDL.Play(mAudioChannel);

            // log statistics about raw PCM audio data stream
            AnnouncePacket(tAudioBufferSize);
        }
    }else
    {
        #ifdef WOPA_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Writing %d samples to audio output stream", pChunkSize / 4);
        #endif

		AUDIOOUTSDL.Enqueue(mAudioChannel, (void*)pChunkBuffer, pChunkSize);
	    AUDIOOUTSDL.Play(mAudioChannel);

        // log statistics about raw PCM audio data stream
        AnnouncePacket(pChunkSize);
    }

	mPlayMutex.unlock();
    return true;
}

void WaveOutSdl::SetVolume(int pValue)
{
	LOG(LOG_VERBOSE, "Setting volume to %d \%", pValue);
	if (pValue > 0)
		AUDIOOUTSDL.SetVolume(mAudioChannel, 128);
	else
		AUDIOOUTSDL.SetVolume(mAudioChannel, 0);
	WaveOut::SetVolume(pValue);
}

bool WaveOutSdl::Play()
{
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

    AUDIOOUTSDL.SetVolume(mAudioChannel, 128);
    AUDIOOUTSDL.Play(mAudioChannel);
    AUDIOOUTSDL.SetVolume(mAudioChannel, 128);

    // unlock grabbing
    mPlayMutex.unlock();

    return true;
}

void WaveOutSdl::Stop()
{
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

    AUDIOOUTSDL.Stop(mAudioChannel);
    AUDIOOUTSDL.ClearChunkList(mAudioChannel);

    // unlock grabbing
    mPlayMutex.unlock();
}

}} //namespaces
