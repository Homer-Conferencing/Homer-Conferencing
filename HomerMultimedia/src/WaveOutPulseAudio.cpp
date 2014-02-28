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
 * Purpose: Wave output based on PulseAudio
 * Author:  Thomas Volkert
 * Since:   2013-02-09
 */

#include <Header_PulseAudio.h>
#include <ProcessStatisticService.h>
#include <WaveOutPulseAudio.h>
#include <MediaSourcePulseAudio.h>
#include <Logger.h>
#include <HBTime.h>

using namespace std;
using namespace Homer::Monitor;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

WaveOutPulseAudio::WaveOutPulseAudio(string pOutputName, string pDesiredDevice):
    WaveOut(pOutputName)
{
    mOutputStream = NULL;
    LOG(LOG_VERBOSE, "Creating wave out for %s on device %s", pOutputName.c_str(), pDesiredDevice.c_str());

    if ((pDesiredDevice != "") && (pDesiredDevice != "auto"))
    {
        bool tNewDeviceSelected = false;
        tNewDeviceSelected = SelectDevice(pDesiredDevice);
        if (!tNewDeviceSelected)
            LOG(LOG_INFO, "Haven't selected new PortAudio device when creating source object");
    }

    LOG(LOG_VERBOSE, "Created");
}

WaveOutPulseAudio::~WaveOutPulseAudio()
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

void WaveOutPulseAudio::getAudioDevices(AudioDevices &pAList)
{
    // This is where we'll store the input device list
    PulseAudioDeviceDescriptor tInputDevicesList[MAX_PULSEAUDIO_DEVICES_IN_LIST];

    // This is where we'll store the output device list
    PulseAudioDeviceDescriptor tOutputDevicesList[MAX_PULSEAUDIO_DEVICES_IN_LIST];

    if (MediaSourcePulseAudio::GetPulseAudioDevices(tInputDevicesList, tOutputDevicesList) < 0)
        LOG(LOG_ERROR, "Couldn't determine the available PulseAudio devices");

    static bool tFirstCall = true;
    AudioDeviceDescriptor tDevice;

    #ifdef WOPUA_DEBUG_PACKETS
        tFirstCall = true;
    #endif

    if (tFirstCall)
    {
        LOG(LOG_VERBOSE, "Enumerating hardware..");
        LOG(LOG_VERBOSE, "PulseAudio version \"%s\"(%d.%d.%d)", pa_get_library_version(), PA_MAJOR, PA_MINOR, PA_MICRO);
    }

    tDevice.Name = "auto";
    tDevice.Card = "";
    tDevice.Desc = "automatic device selection";
    tDevice.IoType = "Output";

    pAList.push_back(tDevice);

    for (int i = 0; i < MAX_PULSEAUDIO_DEVICES_IN_LIST; i++)
    {
        if (!tOutputDevicesList[i].Initialized)
            break;

        tDevice.Name = string(tOutputDevicesList[i].Description);
        tDevice.Card = string(tOutputDevicesList[i].Name);
        tDevice.Desc = "PulseAudio based audio device";
        tDevice.IoType = "Output";
        pAList.push_back(tDevice);

        if (tFirstCall)
        {
            LOG(LOG_VERBOSE, "Output device %d..", i);
            LOG(LOG_VERBOSE, "..name: \"%s\"", tDevice.Name.c_str());
            LOG(LOG_VERBOSE, "..card: \"%s\"", tDevice.Card.c_str());
            LOG(LOG_VERBOSE, "..description: \"%s\"", tDevice.Desc.c_str());
        }
    }

    tFirstCall = false;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool WaveOutPulseAudio::OpenWaveOutDevice(int pSampleRate, int pOutputChannels)
{
    pa_sample_spec     tOutputFormat;
    int                tRes;
    pa_usec_t          tLatency;

    LOG(LOG_VERBOSE, "Trying to open the wave out device");

    if (mWaveOutOpened)
        return false;

    mSampleRate = pSampleRate;
    mAudioChannels = pOutputChannels;

    LOG(LOG_VERBOSE, "Desired device is %s", mDesiredDevice.c_str());

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
    {
        LOG(LOG_VERBOSE, "Using default audio device");
        mDesiredDevice = "";
    }

    tOutputFormat.format = PA_SAMPLE_S16LE;
    tOutputFormat.rate = mSampleRate;
    tOutputFormat.channels = mAudioChannels;

    // create a new playback stream
    if (!(mOutputStream = pa_simple_new(NULL, "Homer-Conferencing", PA_STREAM_PLAYBACK, (mDesiredDevice != "" ? mDesiredDevice.c_str() : NULL) /* dev Name */, GetStreamName().c_str(), &tOutputFormat, NULL, NULL, &tRes)))
    {
        LOG(LOG_ERROR, "Couldn't create PulseAudio stream because %s(%d)", pa_strerror(tRes), tRes);
        return false;
    }

    if ((tLatency = pa_simple_get_latency(mOutputStream, &tRes)) == (pa_usec_t) -1)
    {
        LOG(LOG_ERROR, "Couldn't determine the latency of the output stream because %s(%d)", pa_strerror(tRes), tRes);
        pa_simple_free(mOutputStream);
        mOutputStream = NULL;
        return false;
    }

    mCurrentDevice = mDesiredDevice;

    //######################################################
    //### give some verbose output
    //######################################################
    LOG(LOG_INFO, "PortAudio wave out opened...");
    LOG(LOG_INFO,"    ..sample rate: %d", mSampleRate);
    LOG(LOG_INFO,"    ..channels: %d", pOutputChannels);
    LOG(LOG_INFO,"    ..desired device: %s", mDesiredDevice.c_str());
    LOG(LOG_INFO,"    ..selected device: %s", mCurrentDevice.c_str());
    LOG(LOG_INFO,"    ..latency: %"PRIu64" seconds", (uint64_t)tLatency * 1000 * 1000);
    LOG(LOG_INFO,"    ..sample format: %d", PA_SAMPLE_S16LE);

    mWaveOutOpened = true;

    return true;
}

bool WaveOutPulseAudio::CloseWaveOutDevice()
{
    bool     tResult = false;
    int      tRes;

    LOG(LOG_VERBOSE, "Going to close..");

    if (mWaveOutOpened)
    {
        StopFilePlayback();
        Stop();

        if (mOutputStream != NULL)
        {
            LOG(LOG_VERBOSE, "..draining stream");
            if (pa_simple_drain(mOutputStream, &tRes) < 0)
            {
                LOG(LOG_ERROR, "Couldn't drain the output stream because %s(%d)", pa_strerror(tRes), tRes);
            }
            LOG(LOG_VERBOSE, "..closing stream");
            pa_simple_free(mOutputStream);
        }

        LOG(LOG_INFO, "...closed");

        mWaveOutOpened = false;
        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

     ResetPacketStatistic();

    return tResult;
}

bool WaveOutPulseAudio::Play()
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

    // unlock grabbing
    mPlayMutex.unlock();

    return true;
}

void WaveOutPulseAudio::Stop()
{
    int tRes;

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

    if (mOutputStream != NULL)
    {
        LOG(LOG_VERBOSE, "..draining stream");
        if (pa_simple_drain(mOutputStream, &tRes) < 0)
        {
            LOG(LOG_ERROR, "Couldn't drain the output stream because %s(%d)", pa_strerror(tRes), tRes);
        }
    }

    // unlock grabbing
    mPlayMutex.unlock();
}

bool WaveOutPulseAudio::PulseAudioAvailable()
{
    return MediaSourcePulseAudio::PulseAudioAvailable();
}

void WaveOutPulseAudio::DoWriteChunk(char *pChunkBuffer, int pChunkSize)
{
    int tRes;

    if (!mPlaybackStopped)
    {
        #ifdef WOPUA_DEBUG_TIMING
            int64_t tTime = Time::GetTimeStamp();
        #endif
        if (pa_simple_write(mOutputStream, pChunkBuffer, pChunkSize, &tRes) < 0)
        {
            LOG(LOG_ERROR, "Couldn't write audio chunk of %d bytes to output stream because %s(%d)", pChunkSize, pa_strerror(tRes), tRes);
        }
        #ifdef WOPUA_DEBUG_TIMING
            LOG(LOG_VERBOSE, "PulseAudio-WRITE took %lld ms", (Time::GetTimeStamp() - tTime) / 1000);
        #endif
    }
}

}} //namespaces
