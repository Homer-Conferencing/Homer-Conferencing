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
 * Purpose: CoreAudio capture implementation for OSX
 * Author:  Thomas Volkert
 * Since:   2011-11-16
 */

#include <MediaSourceCoreAudio.h>
#include <ProcessStatisticService.h>
#include <Logger.h>

#include <string.h>
#include <stdlib.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

MediaSourceCoreAudio::MediaSourceCoreAudio(string pDesiredDevice):
    MediaSource("CoreAudio: local capture")
{
    ClassifyStream(DATA_TYPE_AUDIO, PACKET_TYPE_RAW);

    bool tNewDeviceSelected = false;
    SelectDevice(pDesiredDevice, MEDIA_AUDIO, tNewDeviceSelected);
    if (!tNewDeviceSelected)
    {
        LOG(LOG_INFO, "Haven't selected new CoreAudio device when creating source object");
    }

    LOG(LOG_VERBOSE, "Created");
}

MediaSourceCoreAudio::~MediaSourceCoreAudio()
{
    if (mMediaSourceOpened)
        CloseGrabDevice();

    LOG(LOG_VERBOSE, "Destroyed");
}

void MediaSourceCoreAudio::getAudioDevices(AudioDevicesList &pAList)
{
    static bool tFirstCall = true;
    AudioDeviceDescriptor tDevice;

    #ifdef MSCA_DEBUG_PACKETS
        tFirstCall = true;
    #endif

    if (tFirstCall)
        LOG(LOG_VERBOSE, "Enumerating hardware..");

    //TODO: implement

    tFirstCall = false;
}

bool MediaSourceCoreAudio::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    LOG(LOG_ERROR, "Wrong media type");
    return false;
}

bool MediaSourceCoreAudio::OpenAudioGrabDevice(int pSampleRate, bool pStereo)
{
    unsigned int tChannels = pStereo?2:1;
    int tErr;

    LOG(LOG_VERBOSE, "Trying to open the audio source");

    if (mMediaType == MEDIA_VIDEO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return false;
    }

    SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(CoreAudio)");

    if (mMediaSourceOpened)
        return false;

    mSampleRate = pSampleRate;
    mStereo = pStereo;

//    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
//        mDesiredDevice = "default";

    //TODO: implement

//    mSampleBufferSize = MEDIA_SOURCE_ALSA_BUFFER_SIZE * 2 /* SND_PCM_FORMAT_S16_LE */ * tChannels;

    //######################################################
    //### give some verbose output
    //######################################################
    LOG(LOG_INFO, "%s-audio source opened...", "MediaSourceALSA");
    LOG(LOG_INFO,"    ..sample rate: %d", pSampleRate);
    LOG(LOG_INFO,"    ..channels: %d", tChannels);
    LOG(LOG_INFO,"    ..desired device: %s", mDesiredDevice.c_str());
    LOG(LOG_INFO,"    ..selected device: %s", mCurrentDevice.c_str());
//    LOG(LOG_INFO,"    ..access: %d", SND_PCM_ACCESS_RW_INTERLEAVED);
//    LOG(LOG_INFO,"    ..sample format: %d", SND_PCM_FORMAT_S16_LE);
//    LOG(LOG_INFO,"    ..sample buffer size: %d", mSampleBufferSize);

    mChunkNumber = 0;
    mMediaType = MEDIA_AUDIO;
    mMediaSourceOpened = true;

    return true;
}

bool MediaSourceCoreAudio::CloseGrabDevice()
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

        //TODO: implement

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mGrabbingStopped = false;
    mMediaType = MEDIA_UNKNOWN;

    ResetPacketStatistic();

    return tResult;
}

int MediaSourceCoreAudio::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
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

    //TODO: implement

    //pChunkSize = mSampleBufferSize;

    // re-encode the frame and write it to file
//    if (mRecording)
//        RecordSamples((int16_t *)pChunkBuffer, pChunkSize);

    // unlock grabbing
    mGrabMutex.unlock();

    // log statistics about raw PCM audio data stream
//    AnnouncePacket(pChunkSize);

    // acknowledge success
    MarkGrabChunkSuccessful();

    return ++mChunkNumber;
}

string MediaSourceCoreAudio::GetCodecName()
{
    return "Raw";
}

string MediaSourceCoreAudio::GetCodecLongName()
{
    return "Raw";
}

}} //namespaces
