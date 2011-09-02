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
 * Name:    WaveOutAlsa.cpp
 * Purpose: Wave output based on Alsa
 * Author:  Thomas Volkert
 * Since:   2010-12-11
 * Version: $Id$
 */

//HINT: documentation: http://www.alsa-project.org/alsa-doc/alsa-lib/group___p_c_m.html

#include <WaveOutAlsa.h>
#include <Logger.h>

namespace Homer { namespace SoundOutput {

using namespace std;
using namespace Homer::Monitor;

WaveOutAlsa::WaveOutAlsa(string pDesiredDevice):
    WaveOut("ALSA: local playback")
{
    SelectDevice(pDesiredDevice);
    //mSampleBufferSize = MEDIA_SOURCE_ALSA_BUFFER_SIZE * 2 /* SND_PCM_FORMAT_S16_LE */ * 2 /* stereo */;
    mPlaybackHandle = NULL;
    LOG(LOG_VERBOSE, "Created");
}

WaveOutAlsa::~WaveOutAlsa()
{
    if (mWaveOutOpened)
        CloseWaveOutDevice();

    LOG(LOG_VERBOSE, "Destroyed");
}

void WaveOutAlsa::getAudioDevices(AudioOutDevicesList &pAList)
{
    static bool tFirstCall = true;
    AudioOutDeviceDescriptor tDevice;
    void **tDeviceNames;
    size_t tPos, tPos1;

    #ifndef WOA_DEBUG_PACKETS
        tFirstCall = true;
    #endif

    if (tFirstCall)
        LOG(LOG_VERBOSE, "Enumerating hardware..");

    tDevice.Name = "auto";
    tDevice.Card = "";
    tDevice.Desc = "automatic device selection";
    tDevice.IoType = "Input/Output";
    pAList.push_front(tDevice);

    // query all pcm playback devices
    if (snd_device_name_hint(-1, "pcm", &tDeviceNames) < 0)
        return;

    void **tDeviceNamesIt = tDeviceNames;
    while(*tDeviceNamesIt != NULL)
    {
        char* tName = snd_device_name_get_hint(*tDeviceNamesIt, "NAME");
        if (tFirstCall)
            LOG(LOG_VERBOSE, "Got sound device entry: %s", tName);

        string tNameStr = "";
        if (tName != NULL)
            tNameStr = string(tName);

        // search for "default:", if negative result then go on searching for next device
        if ((tNameStr != "") && ((tPos = tNameStr.find("default:")) != string::npos) && ((tPos1 = tNameStr.find("CARD=")) != string::npos))
        {
            tDevice.Card = tNameStr;

            tNameStr.erase(tPos, 8);
            tNameStr.erase(tPos1 - 8, 5);

            tDevice.Name = "ALSA: " + tNameStr;

            char* tDesc = snd_device_name_get_hint(*tDeviceNamesIt, "DESC");
            if (tDesc)
                tDevice.Desc = tDesc;
            else
                tDevice.Desc = "";
            // replace all "enter" with "white space"
            while ((tPos = tDevice.Desc.find('\n')) != string::npos)
                tDevice.Desc[tPos] = ' ';

            char* tIo = snd_device_name_get_hint(*tDeviceNamesIt, "IOID");
            if (tIo)
                tDevice.IoType = tIo;
            else
                tDevice.IoType = "Input/Output";

            if (tFirstCall)
            {
                LOG(LOG_VERBOSE, "Found matching audio device: %s", tDevice.Name.c_str());
                LOG(LOG_VERBOSE, "   ..card: %s", tDevice.Card.c_str());
                LOG(LOG_VERBOSE, "   ..desc.: %s", tDevice.Desc.c_str());
                LOG(LOG_VERBOSE, "   ..i/o-type: %s", tDevice.IoType.c_str());
            }

            // if device is able to capture samples add this device to the result list
            if (tDevice.IoType.find("Output") != string::npos)
                pAList.push_back(tDevice);
        }

        // jump to next alsa device entry
        tDeviceNamesIt++;
    }

    snd_device_name_free_hint(tDeviceNames);
    tFirstCall = false;
}

bool WaveOutAlsa::OpenWaveOutDevice(int pSampleRate, bool pStereo)
{
    unsigned int tChannels = pStereo?2:1;
    int tErr;

    LOG(LOG_VERBOSE, "Trying to open the Alsa device");

    if (mWaveOutOpened)
        return false;

    mSampleRate = pSampleRate;
    mStereo = pStereo;

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
        mDesiredDevice = "default";

    /* snd_pcm_open - parameters:
        pcmp    returned PCM handle
        name    ASCII identifier of the PCM handle
        stream  wanted stream
        mode    open mode (see SND_PCM_NONBLOCK, SND_PCM_ASYNC)
    */
    if ((tErr = snd_pcm_open(&mPlaybackHandle, mDesiredDevice.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        LOG(LOG_ERROR, "Cannot open audio device \"%s\" because of \"%s\"", mDesiredDevice.c_str(), snd_strerror(tErr));
        return false;
    }
    mCurrentDevice = mDesiredDevice;

    /* snd_pcm_set_params - parameters:
        pcm             PCM handle
        format          required PCM format
        access          required PCM access
        channels        required PCM channels
        rate            required sample rate in Hz
        soft_resample   0 = disallow alsa-lib resample stream, 1 = allow resampling
        latency         required overall latency in us
    */
    if ((tErr = snd_pcm_set_params(mPlaybackHandle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, tChannels, (uint)pSampleRate, 1, 500000)) < 0)
    {
        LOG(LOG_ERROR, "Cannot set parameters because of \"%s\"", snd_strerror (tErr));
        return false;
    }

    /* snd_pcm_prepare - parameters:
        pcm             PCM handle
    */
    if ((tErr = snd_pcm_prepare(mPlaybackHandle)) < 0)
    {
        LOG(LOG_ERROR, "Cannot prepare audio device \"%s\" for usage because of \"%s\"", mCurrentDevice.c_str(), snd_strerror(tErr));
        return false;
    }

    //mSampleBufferSize = MEDIA_SOURCE_ALSA_BUFFER_SIZE * 2 /* SND_PCM_FORMAT_S16_LE */ * tChannels;

    LOG(LOG_INFO, "Opened...");
    LOG(LOG_INFO,"    ..sample rate: %d", pSampleRate);
    LOG(LOG_INFO,"    ..channels: %d", tChannels);
    LOG(LOG_INFO,"    ..desired device: %s", mDesiredDevice.c_str());
    LOG(LOG_INFO,"    ..selected device: %s", mCurrentDevice.c_str());
    LOG(LOG_INFO,"    ..access: %d", SND_PCM_ACCESS_RW_INTERLEAVED);
    LOG(LOG_INFO,"    ..sample format: %d", SND_PCM_FORMAT_S16_LE);
    //LOG(LOG_INFO,"    ..sample buffer size: %d", mSampleBufferSize);

    mChunkNumber = 0;
    mWaveOutOpened = true;
    return true;
}

bool WaveOutAlsa::CloseWaveOutDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close");

    if (mWaveOutOpened)
    {
        mWaveOutOpened = false;

        snd_pcm_close(mPlaybackHandle);

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    //mGrabbingStopped = false;

    return tResult;
}

bool WaveOutAlsa::WriteChunk(void* pChunkBuffer, int pChunkSize)
{
    int tResult;
    int tFramesCount = pChunkSize / SND_PCM_FORMAT_S16_LE / (mStereo ? 2 : 1);

    if (mPlaybackStopped)
    {
        LOG(LOG_ERROR, "Tried to play while WaveOut device is paused");
        return -1;
    }

    if (!mWaveOutOpened)
    {
        //LOG(LOG_ERROR, "Tried to play while WaveOut device is closed");
        return -1;
    }

    if (((tResult = snd_pcm_writei(mPlaybackHandle, pChunkBuffer, tFramesCount)) != tFramesCount) && (!mPlaybackStopped))
    {// insufficient data was grabbed or error occurred, but grabbing wasn't stopped yet
        LOG(LOG_INFO, "Can not write enough audio samples to playback device because of \"%s\", return value was: %d", snd_strerror(tResult), tResult);

        if ((tResult = snd_pcm_recover(mPlaybackHandle, tResult, 0)) < 0)
        {
            LOG(LOG_ERROR, "Can not recover from failure state for audio device \"%s\" and remain with error \"%s\"", mCurrentDevice.c_str(), snd_strerror(tResult));
            return -1;
        }

        return -1;
    }

    // log statistics about raw PCM audio data stream
    AnnouncePacket(pChunkSize);

    return ++mChunkNumber;
}

void WaveOutAlsa::StopPlayback()
{
    WaveOut::StopPlayback();

    int tRes = 0;
    if ((tRes = snd_pcm_drop(mPlaybackHandle)) < 0)
        LOG(LOG_INFO, "Couldn't stop audio playback because of \"%s\", return value was: %d", snd_strerror(tRes), tRes);
}

}} //namespaces
