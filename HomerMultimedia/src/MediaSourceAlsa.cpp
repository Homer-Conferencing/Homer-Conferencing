/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
 * Copyright (C) 2009-2009 Stefan Koegel
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
 * Purpose: Alsa Capture Implementation
 * Author:  Stefan Kögel, Thomas Volkert
 * Since:   2009-05-18
 */

//HINT: documentation: http://www.alsa-project.org/alsa-doc/alsa-lib/group___p_c_m.html

#include <MediaSourceAlsa.h>
#include <ProcessStatisticService.h>
#include <Logger.h>

#include <string.h>
#include <stdlib.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

MediaSourceAlsa::MediaSourceAlsa(string pDesiredDevice):
    MediaSource("ALSA: local capture")
{
    ClassifyStream(DATA_TYPE_AUDIO, PACKET_TYPE_RAW);
    mSampleBufferSize = MEDIA_SOURCE_ALSA_BUFFER_SIZE * 2 /* SND_PCM_FORMAT_S16_LE */ * 2 /* stereo */;
    mCaptureHandle = NULL;

    bool tNewDeviceSelected = false;
    SelectDevice(pDesiredDevice, MEDIA_AUDIO, tNewDeviceSelected);
    if (!tNewDeviceSelected)
    {
        LOG(LOG_INFO, "Haven't selected new ALSA device when creating source object");
    }

    LOG(LOG_VERBOSE, "Created");
}

MediaSourceAlsa::~MediaSourceAlsa()
{
    if (mMediaSourceOpened)
        CloseGrabDevice();

    LOG(LOG_VERBOSE, "Destroyed");
}

void MediaSourceAlsa::getAudioDevices(AudioDevicesList &pAList)
{
    static bool tFirstCall = true;
    AudioDeviceDescriptor tDevice;
    void **tDeviceNames;
    size_t tPos, tPos1;

    #ifdef MSA_DEBUG_PACKETS
        tFirstCall = true;
    #endif

    if (tFirstCall)
        LOG(LOG_VERBOSE, "Enumerating hardware..");

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
                if (tDevice.Name.size())
                {
                    LOG(LOG_VERBOSE, "Found matching audio capture device: %s", tDevice.Name.c_str());
                    LOG(LOG_VERBOSE, "   ..card: %s", tDevice.Card.c_str());
                    LOG(LOG_VERBOSE, "   ..desc.: %s", tDevice.Desc.c_str());
                    LOG(LOG_VERBOSE, "   ..i/o-type: %s", tDevice.IoType.c_str());
                }
            }

            // if device is able to capture samples add this device to the result list
            if (tDevice.IoType.find("Input") != string::npos)
                pAList.push_back(tDevice);
        }

        // jump to next alsa device entry
        tDeviceNamesIt++;
    }

    snd_device_name_free_hint(tDeviceNames);
    tFirstCall = false;
}

bool MediaSourceAlsa::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    LOG(LOG_ERROR, "Wrong media type");
    return false;
}

bool MediaSourceAlsa::OpenAudioGrabDevice(int pSampleRate, bool pStereo)
{
    unsigned int tChannels = pStereo?2:1;
    int tErr;

    LOG(LOG_VERBOSE, "Trying to open the audio source");

    if (mMediaType == MEDIA_VIDEO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return false;
    }

    SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(ALSA)");

    if (mMediaSourceOpened)
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
    if ((tErr = snd_pcm_open(&mCaptureHandle, mDesiredDevice.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0)
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
    if ((tErr = snd_pcm_set_params(mCaptureHandle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, tChannels, (uint)pSampleRate, 1, 500000)) < 0)
    {
        LOG(LOG_ERROR, "Cannot set parameters because of \"%s\"", snd_strerror (tErr));
        return false;
    }

    /* snd_pcm_prepare - parameters:
        pcm             PCM handle
    */
    if ((tErr = snd_pcm_prepare(mCaptureHandle)) < 0)
    {
        LOG(LOG_ERROR, "Cannot prepare audio device \"%s\" for usage because of \"%s\"", mCurrentDevice.c_str(), snd_strerror(tErr));
        return false;
    }

    mSampleBufferSize = MEDIA_SOURCE_ALSA_BUFFER_SIZE * 2 /* SND_PCM_FORMAT_S16_LE */ * tChannels;

    //######################################################
    //### give some verbose output
    //######################################################
    LOG(LOG_INFO, "%s-audio source opened...", "MediaSourceALSA");
    LOG(LOG_INFO,"    ..sample rate: %d", pSampleRate);
    LOG(LOG_INFO,"    ..channels: %d", tChannels);
    LOG(LOG_INFO,"    ..desired device: %s", mDesiredDevice.c_str());
    LOG(LOG_INFO,"    ..selected device: %s", mCurrentDevice.c_str());
    LOG(LOG_INFO,"    ..access: %d", SND_PCM_ACCESS_RW_INTERLEAVED);
    LOG(LOG_INFO,"    ..sample format: %d", SND_PCM_FORMAT_S16_LE);
    LOG(LOG_INFO,"    ..sample buffer size: %d", mSampleBufferSize);

    mChunkNumber = 0;
    mMediaType = MEDIA_AUDIO;
    mMediaSourceOpened = true;

    return true;
}

bool MediaSourceAlsa::CloseGrabDevice()
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

        mMediaSourceOpened = false;

        snd_pcm_close(mCaptureHandle);

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mGrabbingStopped = false;
    mMediaType = MEDIA_UNKNOWN;

    ResetPacketStatistic();

    return tResult;
}

int MediaSourceAlsa::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    int tResult;

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

    if (((tResult = snd_pcm_readi(mCaptureHandle, pChunkBuffer, MEDIA_SOURCE_ALSA_BUFFER_SIZE)) != MEDIA_SOURCE_ALSA_BUFFER_SIZE) && (!mGrabbingStopped))
    {// insufficient data was grabbed or error occurred, but grabbing wasn't stopped yet
        LOG(LOG_INFO, "Can not grab enough audio samples because of \"%s\", return value was: %d", snd_strerror(tResult), tResult);

        if ((tResult = snd_pcm_recover(mCaptureHandle, tResult, 0)) < 0)
        {
            //TODO: recover schlaegt fehl, insofern StandBy ausgeloest wurde und PC wieder daraus aufgewacht ist

            // acknowledge failed"
            MarkGrabChunkFailed("recover error from failure state for audio device \"" + mCurrentDevice + "\", error is \"" + toString(snd_strerror(tResult)) + "\"");

            return GRAB_RES_INVALID;
        }

        // unlock grabbing
        mGrabMutex.unlock();

        return GRAB_RES_INVALID;
    }

    pChunkSize = mSampleBufferSize;

    // re-encode the frame and write it to file
    if (mRecording)
        RecordSamples((int16_t *)pChunkBuffer, pChunkSize);

    // unlock grabbing
    mGrabMutex.unlock();

    // log statistics about raw PCM audio data stream
    AnnouncePacket(pChunkSize);

    // acknowledge success
    MarkGrabChunkSuccessful();

    return ++mChunkNumber;
}

bool MediaSourceAlsa::SupportsRecording()
{
	return true;
}

void MediaSourceAlsa::StopGrabbing()
{
	MediaSource::StopGrabbing();

	if (mMediaSourceOpened)
	{
        int tRes = 0;
        if ((tRes = snd_pcm_drop(mCaptureHandle)) < 0)
            LOG(LOG_INFO, "Couldn't stop audio grabbing because of \"%s\", return value was: %d", snd_strerror(tRes), tRes);
	}
}

string MediaSourceAlsa::GetCodecName()
{
    return "Raw";
}

string MediaSourceAlsa::GetCodecLongName()
{
    return "Raw";
}

}} //namespaces
