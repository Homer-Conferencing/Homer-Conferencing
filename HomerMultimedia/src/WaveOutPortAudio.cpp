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


#include <WaveOutPortAudio.h>
#include <Logger.h>
#include <HBThread.h>

using namespace std;

namespace Homer { namespace Multimedia {

Mutex WaveOutPortAudio::mPaInitMutex;
bool WaveOutPortAudio::mPaInitiated = false;

WaveOutPortAudio::WaveOutPortAudio(string pDesiredDevice):
    WaveOut("Audio playback")
{
    mPaInitMutex.lock();
    if (!mPaInitiated)
    {
        // initialize portaudio library
        LOG(LOG_VERBOSE, "Initiated portaudio with result: %d", Pa_Initialize());
        mPaInitiated = true;
    }
    mPaInitMutex.unlock();

    if (pDesiredDevice != "")
    {
        bool tNewDeviceSelected = false;
        tNewDeviceSelected = SelectDevice(pDesiredDevice);
        if (!tNewDeviceSelected)
            LOG(LOG_INFO, "Haven't selected new PortAudio device when creating source object");
    }
    LOG(LOG_VERBOSE, "Created");
}

WaveOutPortAudio::~WaveOutPortAudio()
{
    if (mWaveOutOpened)
    {
        Stop();
        CloseWaveOutDevice();
    }

    LOG(LOG_VERBOSE, "Destroyed");
}

void WaveOutPortAudio::getAudioDevices(AudioDevicesList &pAList)
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

        // if device is able to capture samples add this device to the result list
        if (tDevice.IoType.find("Output") != string::npos)
            pAList.push_back(tDevice);

        if (tFirstCall)
        {
            LOG(LOG_VERBOSE, "Device %d..", i );

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

bool WaveOutPortAudio::OpenWaveOutDevice(int pSampleRate, bool pStereo)
{
    unsigned int tChannels = pStereo?2:1;
    PaError           tErr = paNoError;
    PaStreamParameters      tOutputParameters;

    LOG(LOG_VERBOSE, "Trying to open the wave out device");

    if (mWaveOutOpened)
        return false;

    mSampleRate = pSampleRate;
    mStereo = pStereo;

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
    {
        LOG(LOG_VERBOSE, "Using default audio device");
        mDesiredDevice = char(Pa_GetDefaultOutputDevice() + 48);
    }

    int tDeviceId= mDesiredDevice[0] - 48;
    LOG(LOG_VERBOSE, "Will open port audio device %d", tDeviceId);

    tOutputParameters.device = tDeviceId;
    tOutputParameters.channelCount = tChannels;
    tOutputParameters.sampleFormat = paInt16;
    tOutputParameters.suggestedLatency = Pa_GetDeviceInfo(tOutputParameters.device)->defaultLowOutputLatency;
    tOutputParameters.hostApiSpecificStreamInfo = NULL;

    LOG(LOG_VERBOSE, "Going to open stream..");
    LOG(LOG_VERBOSE, "..selected sample rate: %d", mSampleRate);
    if((tErr = Pa_OpenStream(&mStream, NULL /* input parameters */, &tOutputParameters, mSampleRate, SOUNDOUTPUT_SAMPLES_PER_BUFFER, paClipOff | paDitherOff, NULL /* no callback, use blocking API */, NULL)) != paNoError)
    {
        LOG(LOG_ERROR, "Couldn't open stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
        return false;
    }

    mCurrentDevice = mDesiredDevice;

    // init fifo buffer
    mSampleFifo = HM_av_fifo_alloc(SOUNDOUTPUT_SAMPLES_PER_BUFFER * 4 * 4);

    //######################################################
    //### give some verbose output
    //######################################################
    LOG(LOG_INFO, "Wave out opened...");
    LOG(LOG_INFO,"    ..sample rate: %d", mSampleRate);
    LOG(LOG_INFO,"    ..channels: %d", tChannels);
    LOG(LOG_INFO,"    ..desired device: %s", mDesiredDevice.c_str());
    LOG(LOG_INFO,"    ..selected device: %s", mCurrentDevice.c_str());
    LOG(LOG_INFO,"    ..suggested latency: %f seconds", tOutputParameters.suggestedLatency);
    LOG(LOG_INFO,"    ..sample format: %d", paInt16);
    //LOG(LOG_INFO,"    ..sample buffer size: %d", mSampleBufferSize);
    LOG(LOG_INFO, "Fifo opened...");
    LOG(LOG_INFO, "    ..fill size: %d bytes", av_fifo_size(mSampleFifo));

    mWaveOutOpened = true;

    Play();

    return true;
}

bool WaveOutPortAudio::CloseWaveOutDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close");

    if (mWaveOutOpened)
    {
        mWaveOutOpened = false;

        Stop();

        // wait until the port audio stream becomes inactive
        while((Pa_IsStreamActive(mStream)) == 1)
        {
            // wait some time
            Thread::Suspend(250 * 1000);
        }

        PaError tErr = paNoError;
        if ((tErr = Pa_CloseStream(mStream)) != paNoError)
        {
            LOG(LOG_ERROR, "Couldn't close stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
        }

        // free fifo buffer
        av_fifo_free(mSampleFifo);

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

     ResetPacketStatistic();

    return tResult;
}

bool WaveOutPortAudio::WriteChunk(void* pChunkBuffer, int pChunkSize)
{
    PaError           tErr = paNoError;

    mPlayMutex.lock();

    if (!mWaveOutOpened)
    {
        LOG(LOG_ERROR, "Tried to play while WaveOut device is closed");
        mPlayMutex.unlock();
        return false;
    }

    if (mPlaybackStopped)
    {
		LOG(LOG_VERBOSE, "Will automatically start the audio stream");
		mPlayMutex.unlock();
		Play();
		mPlayMutex.lock();
    }

    if (mVolume != 100)
    {
        short int *tSamples = (short int*)pChunkBuffer;
        for (int i = 0; pChunkSize / 4; i++)
        {
            int tNewSample = tSamples[i] * (mVolume / 100);
            if (tNewSample < -32767)
                tNewSample = -32767;
            if (tNewSample >  32767)
                tNewSample =  32767;
            tSamples[i] = tNewSample;
        }
    }
    #ifdef WOPA_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Got %d samples for audio output stream", pChunkSize / 4);
    #endif
//    if (pChunkSize / 4 != SOUNDOUTPUT_SAMPLES_PER_BUFFER)
//    {
//        #ifdef WOPA_DEBUG_PACKETS
//            LOG(LOG_VERBOSE, "Will use FIFO because input chunk has wrong size, need %d samples per chunk buffer", SOUNDOUTPUT_SAMPLES_PER_BUFFER);
//        #endif
//        if (av_fifo_realloc2(mSampleFifo, av_fifo_size(mSampleFifo) + pChunkSize) < 0)
//        {
//            LOG(LOG_ERROR, "Reallocation of FIFO audio buffer failed");
//            return false;
//        }
//
//        // write new samples into fifo buffer
//        av_fifo_generic_write(mSampleFifo, pChunkBuffer, pChunkSize, NULL);
//
//        while (av_fifo_size(mSampleFifo) >= 4 * SOUNDOUTPUT_SAMPLES_PER_BUFFER)
//        {
//            pChunkSize = 4 * SOUNDOUTPUT_SAMPLES_PER_BUFFER;
//
//            // read sample data from the fifo buffer
//            HM_av_fifo_generic_read(mSampleFifo, (void*)pChunkBuffer, pChunkSize);
//
//            #ifdef WOPA_DEBUG_PACKETS
//                LOG(LOG_VERBOSE, "Writing %d samples to audio output stream", pChunkSize / 4);
//            #endif
//            if((tErr = Pa_WriteStream(mStream, pChunkBuffer, pChunkSize / 4)) != paNoError)
//            {
//                LOG(LOG_ERROR, "Couldn't write chunk to stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
//                return false;
//            }
//
//            // log statistics about raw PCM audio data stream
//            AnnouncePacket(pChunkSize);
//        }
//    }else
//    {
        #ifdef WOPA_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Writing %d samples to audio output stream", pChunkSize / 4);
        #endif
        if((tErr = Pa_WriteStream(mStream, pChunkBuffer, pChunkSize / 4)) != paNoError)
        {
        	if (tErr == paStreamIsStopped)
        	{
        		LOG(LOG_VERBOSE, "Will automatically start the audio stream");
        		Play();
                tErr = Pa_WriteStream(mStream, pChunkBuffer, pChunkSize / 4);
        	}
        	if(tErr != paNoError)
        	{
        		LOG(LOG_ERROR, "Couldn't write chunk to stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
        		mPlayMutex.unlock();
        		return false;
        	}
        }

        // log statistics about raw PCM audio data stream
        AnnouncePacket(pChunkSize);
//    }

	mPlayMutex.unlock();
    return true;
}

bool WaveOutPortAudio::Play()
{
    PaError           tErr = paNoError;

    LOG(LOG_VERBOSE, "Starting playback stream..");

    mPlayMutex.lock();

    if (!mWaveOutOpened)
    {
        LOG(LOG_VERBOSE, "Playback device wasn't opened yet");
        mPlayMutex.unlock();
        return true;
    }

    if (!mPlaybackStopped)
    {
        LOG(LOG_VERBOSE, "Playback was already started");
        mPlayMutex.unlock();
        return true;
    }

    // do we already play?
    if (Pa_IsStreamStopped(mStream) == 1)
    {
        LOG(LOG_VERBOSE, "Going to start stream..");
        if((tErr = Pa_StartStream(mStream)) != paNoError)
        {
            LOG(LOG_ERROR, "Couldn't start stream because \"%s\"(%d)", Pa_GetErrorText(tErr), tErr);
            mPlayMutex.unlock();
            return false;
        }

        // wait until the port audio stream becomes active
        int tLoop = 0;
        while((Pa_IsStreamStopped(mStream)) == 1)
        {
        	if (tLoop > 10)
        	{
        		LOG(LOG_WARN, "Was not able to start playback stream");
        	    mPlayMutex.unlock();
        	    return false;
        	}
        	LOG(LOG_VERBOSE, "Wait for stream start, loop count: %d", tLoop);
        	tLoop++;
            // wait some time
            Thread::Suspend(50 * 1000);
        }
    }else
    	LOG(LOG_VERBOSE, "Stream is already started");

    WaveOut::Play();

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
        LOG(LOG_VERBOSE, "Playback device wasn't opened yet");
        mPlayMutex.unlock();
        return;
    }

    if (mPlaybackStopped)
    {
        LOG(LOG_VERBOSE, "Playback was already stopped");
        mPlayMutex.unlock();
        return;
    }

    if (Pa_IsStreamStopped(mStream) == 0)
    {
        LOG(LOG_VERBOSE, "Going to stop stream..");
        if ((tErr = Pa_StopStream(mStream)) != paNoError)
        {
            LOG(LOG_ERROR, "Couldn't stop stream because \"%s\"", Pa_GetErrorText(tErr));
            mPlayMutex.unlock();
            return;
        }

        // wait until the port audio stream becomes inactive
        int tLoop = 0;
        while((Pa_IsStreamStopped(mStream)) == 0)
        {
        	if (tLoop > 10)
        	{
        		LOG(LOG_WARN, "Was not able to stop playback stream");
        	    mPlayMutex.unlock();
        	    return;
        	}
        	LOG(LOG_VERBOSE, "Wait for stream stop, loop count: %d", tLoop);
        	tLoop++;
            // wait some time
            Thread::Suspend(50 * 1000);
        }
    }else
    	LOG(LOG_VERBOSE, "Stream is already stopped");

    WaveOut::Stop();

    mPlayMutex.unlock();
}

}} //namespaces
