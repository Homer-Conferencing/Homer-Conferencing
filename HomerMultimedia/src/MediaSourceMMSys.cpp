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
 * Purpose: Implementation of a ffmpeg based local MMSys video source
 * Since:   2010-10-29
 */

// HINT: documentation: http://msdn.microsoft.com/en-us/library/aa909811.aspx

#include <MediaSourceMMSys.h>
#include <ProcessStatisticService.h>
#include <Logger.h>

#include <cstdio>
#include <string.h>
#include <stdlib.h>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

MediaSourceMMSys::MediaSourceMMSys(string pDesiredDevice):
    MediaSource("MMSys: local capture")
{
    mSourceType = SOURCE_DEVICE;
    // set category for packet statistics
    ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);

    mDesiredDevice = toString((int)WAVE_MAPPER);
    mSampleBufferSize = MEDIA_SOURCE_SAMPLES_BUFFER_SIZE;
    mCaptureHandle = NULL;

    bool tNewDeviceSelected = false;
    SelectDevice(pDesiredDevice, MEDIA_AUDIO, tNewDeviceSelected);
    if (!tNewDeviceSelected)
    {
        LOG(LOG_INFO, "Haven't selected new mmsys device when creating source object");
    }

    LOG(LOG_VERBOSE, "Created");
}

MediaSourceMMSys::~MediaSourceMMSys()
{
    if (mMediaSourceOpened)
        CloseGrabDevice();
}

void MediaSourceMMSys::getAudioDevices(AudioDevices &pAList)
{
    static bool tFirstCall = true;
    AudioDeviceDescriptor tDevice;
    WAVEINCAPS tCaps;
    UINT tCapsSize = sizeof(tCaps);

    #ifdef MMSYS_DEBUG_PACKETS
        tFirstCall = true;
    #endif

    if (tFirstCall)
        LOG(LOG_VERBOSE, "Enumerating hardware..");

    MMRESULT tRes;
    for (UINT i = 0; i < 20; i++)
    {
        if (waveInGetDevCaps(i, (LPWAVEINCAPS)&tCaps, tCapsSize) == MMSYSERR_NOERROR)
        {
            tDevice.Name = string(tCaps.szPname);
            tDevice.Card = (char)i + 48;
            tDevice.Desc = "Windows multimedia system (MMSYS) audio device " + tDevice.Card + " \"" + string(tCaps.szPname) + "\"";

            if (tFirstCall)
            {
                LOG(LOG_INFO, "Found active MMSystem device %d", i);
                LOG(LOG_INFO, "  ..manufacturer identifier: %d", (int)tCaps.wMid);
                LOG(LOG_INFO, "  ..product identifier: %d", (int)tCaps.wPid);
                LOG(LOG_INFO, "  ..driver version: 0x%04X", (int)tCaps.vDriverVersion);
                LOG(LOG_INFO, "  ..product name: %s", (int)tCaps.szPname);
                LOG(LOG_INFO, "  ..channels: %d", (int)tCaps.wChannels);

                if (tCaps.dwFormats & WAVE_FORMAT_1M08)
                    LOG(LOG_INFO, "  ..supporting 11.025 kHz, mono, 8-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_1M16)
                    LOG(LOG_INFO, "  ..supporting 11.025 kHz, mono, 16-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_1S08)
                    LOG(LOG_INFO, "  ..supporting 11.025 kHz, stereo, 8-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_1S16)
                    LOG(LOG_INFO, "  ..supporting 11.025 kHz, stereo, 16-bit");

                if (tCaps.dwFormats & WAVE_FORMAT_2M08)
                    LOG(LOG_INFO, "  ..supporting 22.05 kHz, mono, 8-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_2M16)
                    LOG(LOG_INFO, "  ..supporting 22.05 kHz, mono, 16-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_2S08)
                    LOG(LOG_INFO, "  ..supporting 22.05 kHz, stereo, 8-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_2S16)
                    LOG(LOG_INFO, "  ..supporting 22.05 kHz, stereo, 16-bit");

                if (tCaps.dwFormats & WAVE_FORMAT_4M08)
                    LOG(LOG_INFO, "  ..supporting 44.1 kHz, mono, 8-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_4M16)
                    LOG(LOG_INFO, "  ..supporting 44.1 kHz, mono, 16-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_4S08)
                    LOG(LOG_INFO, "  ..supporting 44.1 kHz, stereo, 8-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_4S16)
                    LOG(LOG_INFO, "  ..supporting 44.1 kHz, stereo, 16-bit");
                /*
                if (tCaps.dwFormats & WAVE_FORMAT_48M08)
                    LOG(LOG_INFO, "  ..supporting 48 kHz, mono, 8-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_48M16)
                    LOG(LOG_INFO, "  ..supporting 48 kHz, mono, 16-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_48S08)
                    LOG(LOG_INFO, "  ..supporting 48 kHz, stereo, 8-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_48S16)
                    LOG(LOG_INFO, "  ..supporting 48 kHz, stereo, 16-bit");

                if (tCaps.dwFormats & WAVE_FORMAT_96M08)
                    LOG(LOG_INFO, "  ..supporting 96 kHz, mono, 8-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_96M16)
                    LOG(LOG_INFO, "  ..supporting 96 kHz, mono, 16-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_96S08)
                    LOG(LOG_INFO, "  ..supporting 96 kHz, stereo, 8-bit");
                if (tCaps.dwFormats & WAVE_FORMAT_96S16)
                    LOG(LOG_INFO, "  ..supporting 96 kHz, stereo, 16-bit");
                 */

                LOG(LOG_VERBOSE, "Found audio capture device: %s (card: %s)", tDevice.Name.c_str(), tDevice.Card.c_str());
            }

            pAList.push_back(tDevice);
        }else
            break;
    }
    tFirstCall = false;
}

///////////////////////////////////////////////////////////////////////////////

bool MediaSourceMMSys::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    LOG(LOG_ERROR, "Wrong media type");
    return false;
}

bool MediaSourceMMSys::OpenAudioGrabDevice(int pSampleRate, int pChannels)
{
    int tErr;
    MMRESULT tResult;
    WAVEFORMATEX tFormat;
    char tErrorBuffer[256];

    mMediaType = MEDIA_AUDIO;
    mOutputAudioChannels = pChannels;
    mOutputAudioSampleRate = pSampleRate;
    mOutputAudioFormat = AV_SAMPLE_FMT_S16; // assume we always want signed 16 bit

    LOG(LOG_VERBOSE, "Trying to open the audio source");

    SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(MMSYS)");

    if (mMediaSourceOpened)
        return false;

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
        mDesiredDevice = toString((int)WAVE_MAPPER);

    // Specify recording parameters
    tFormat.wFormatTag = WAVE_FORMAT_PCM;   // simple, uncompressed format
    tFormat.nChannels = mOutputAudioChannels;
    tFormat.nSamplesPerSec = mOutputAudioSampleRate;   // default: 44100
    tFormat.nAvgBytesPerSec = mOutputAudioSampleRate * 2 * 2; // SamplesPerSec * Channels * BitsPerSample / 8
    tFormat.nBlockAlign = 2 * 2;            // Channels * BitsPerSample / 8
    tFormat.wBitsPerSample = 16;            //  16 for high quality, 8 for telephone quality
    tFormat.cbSize = 0;                     // no additional information appended

    LOG(LOG_VERBOSE, "Check if format is supported..");
    tResult = waveInOpen(&mCaptureHandle, (UINT)atoi(mDesiredDevice.c_str()), &tFormat, 0L, 0L, WAVE_FORMAT_QUERY);
    if(tResult == WAVERR_BADFORMAT)
    {
        waveInGetErrorText(tResult, tErrorBuffer, 256);
        LOG(LOG_ERROR, "Selected wave format isn't support because of \"%s\"", tErrorBuffer);
        return false;
    }

    LOG(LOG_VERBOSE, "Open WaveIn device..");
    tResult = waveInOpen(&mCaptureHandle, (UINT)atoi(mDesiredDevice.c_str()), &tFormat, (DWORD)&EventHandler, (DWORD)this, CALLBACK_FUNCTION | WAVE_FORMAT_DIRECT);
    if(tResult != MMSYSERR_NOERROR)
    {
        waveInGetErrorText(tResult, tErrorBuffer, 256);
        LOG(LOG_ERROR, "Can not open WaveIn device because of \"%s\"", tErrorBuffer);
        return false;
    }

    mCurrentDevice = mDesiredDevice;
    mSampleBufferSize = MEDIA_SOURCE_SAMPLES_BUFFER_SIZE;

    for (int i = 0; i < MEDIA_SOURCE_MMSYS_BUFFER_AMOUNT; i++)
    {
        mCaptureBuffer[i] = (char*)malloc(MEDIA_SOURCE_SAMPLES_BUFFER_SIZE);
        mCaptureBufferDesc[i].lpData = (LPSTR)mCaptureBuffer[i];
        mCaptureBufferDesc[i].dwBufferLength = MEDIA_SOURCE_SAMPLES_BUFFER_SIZE;
        mCaptureBufferDesc[i].dwBytesRecorded = 0;
        mCaptureBufferDesc[i].dwUser = i;
        mCaptureBufferDesc[i].dwFlags = 0L;
        mCaptureBufferDesc[i].dwLoops = 0L;
        #ifdef MMSYS_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Preparing buffer descriptor %d at %p.. with capture buffer at %p", i, &mCaptureBufferDesc[i], mCaptureBuffer[i]);
        #endif
        // prepare buffer descriptor for usage for audio capturing
        tResult = waveInPrepareHeader(mCaptureHandle, &mCaptureBufferDesc[i], sizeof(WAVEHDR));
        if(tResult != MMSYSERR_NOERROR)
        {
            waveInGetErrorText(tResult, tErrorBuffer, 256);
            LOG(LOG_ERROR, "Can not prepare capture buffer descriptor %d for WaveIn device because of \"%s\"", i, tErrorBuffer);
            return false;
        }

        #ifdef MMSYS_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Adding capture buffer %d..", i);
        #endif
        // insert new buffer in device queue
        tResult = waveInAddBuffer(mCaptureHandle, &mCaptureBufferDesc[i], sizeof(WAVEHDR));
        if(tResult != MMSYSERR_NOERROR)
        {
            waveInGetErrorText(tResult, tErrorBuffer, 256);
            LOG(LOG_ERROR, "Can not register capture buffer %d for WaveIn device because of \"%s\"", i, tErrorBuffer);
            return false;
        }
    }

    for (int i = 0; i < MEDIA_SOURCE_MMSYS_BUFFER_QUEUE_SIZE; i++)
    {
        mQueue[i].Size = 0;
        mQueue[i].Data = (char*)malloc(MEDIA_SOURCE_SAMPLES_BUFFER_SIZE);
    }

    #ifdef MMSYS_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Starting capturing..");
    #endif
    // commence sampling input
    tResult = waveInStart(mCaptureHandle);
    if(tResult != MMSYSERR_NOERROR)
    {
        waveInGetErrorText(tResult, tErrorBuffer, 256);
        LOG(LOG_ERROR, "Can not start audio capturing on WaveIn device because of \"%s\"", tErrorBuffer);
        return false;
    }

    mInputFrameRate = (float)mOutputAudioSampleRate /* 44100 samples per second */ / MEDIA_SOURCE_SAMPLES_PER_BUFFER /* 1024 samples per frame */;
    mOutputFrameRate = mInputFrameRate;

    //######################################################
    //### give some verbose output
    //######################################################
    LOG(LOG_INFO, "%s-audio source opened...", "MediaSourceMMSys");
    LOG(LOG_INFO,"    ..sample rate: %d", mOutputAudioSampleRate);
    LOG(LOG_INFO,"    ..channels: %d", mOutputAudioChannels);
    LOG(LOG_INFO,"    ..desired device: %s", mDesiredDevice.c_str());
    LOG(LOG_INFO,"    ..selected device: %s", mCurrentDevice.c_str());
    LOG(LOG_INFO,"    ..sample format: 16 bit little endian");
    LOG(LOG_INFO,"    ..sample buffer size: %d", mSampleBufferSize);

    mQueueWritePtr = 0;
    mQueueReadPtr = 0;
    mQueueSize = 0;
    mMediaType = MEDIA_AUDIO;
    mFrameNumber = 0;
    mMediaSourceOpened = true;

    return true;
}

bool MediaSourceMMSys::CloseGrabDevice()
{
    MMRESULT tResult = 0;
    char tErrorBuffer[256];

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

        LOG(LOG_VERBOSE, "Stopping capturing..");
        // commence sampling input
        tResult = waveInStop(mCaptureHandle);
        if(tResult != MMSYSERR_NOERROR)
        {
            waveInGetErrorText(tResult, tErrorBuffer, 256);
            LOG(LOG_ERROR, "Can not stop audio capturing on WaveIn device because of \"%s\"", tErrorBuffer);
            return false;
        }

        LOG(LOG_VERBOSE, "Going to close WaveIn device");
        tResult = waveInClose(mCaptureHandle);
        if(tResult != MMSYSERR_NOERROR)
        {
            waveInGetErrorText(tResult, tErrorBuffer, 256);
            LOG(LOG_ERROR, "Can not close WaveIn device because of \"%s\"", tErrorBuffer);
            return false;
        }

        LOG(LOG_VERBOSE, "Going to release capture buffers");
        for (int i = 0; i < MEDIA_SOURCE_MMSYS_BUFFER_AMOUNT; i++)
        {
            free(mCaptureBuffer[i]);
        }

        for (int i = 0; i < MEDIA_SOURCE_MMSYS_BUFFER_QUEUE_SIZE; i++)
        {
            mQueue[i].Size = 0;
            free(mQueue[i].Data);
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

void CALLBACK MediaSourceMMSys::EventHandler(HWAVEIN pCapturDevice, UINT pMessage, DWORD pInstance, DWORD pParam1, DWORD pParam2)
{
    MediaSourceMMSys *tSource = (MediaSourceMMSys*)pInstance;
    WAVEHDR *tBufferDesc;
    MMRESULT tResult;
    char tErrorBuffer[256];

    if (tSource == NULL)
    {
        LOGEX(MediaSourceMMSys, LOG_ERROR, "Could not determine the original source of the call back");
        return;
    }

    switch(pMessage)
    {
        case WIM_CLOSE:
            #ifdef MMSYS_DEBUG_PACKETS
                LOGEX(MediaSourceMMSys, LOG_VERBOSE, "################## WaveIn event of type \"device closed\" occurred ######################");
            #endif
            break;
        case WIM_OPEN:
            #ifdef MMSYS_DEBUG_PACKETS
                LOGEX(MediaSourceMMSys, LOG_VERBOSE, "################## WaveIn event of type \"device opened\" occurred ######################");
            #endif
            break;
        case WIM_DATA:
            tBufferDesc = (WAVEHDR*)pParam1;
            if (tBufferDesc == NULL)
            {
                LOGEX(MediaSourceMMSys, LOG_ERROR, "Delivered buffer descriptor is invalid");
                break;
            }
            #ifdef MMSYS_DEBUG_PACKETS
                LOGEX(MediaSourceMMSys, LOG_VERBOSE, "################## WaveIn event of type \"capture data\" occurred #######################");
                LOGEX(MediaSourceMMSys, LOG_VERBOSE, "  ..instance: %p", pInstance);
                LOGEX(MediaSourceMMSys, LOG_VERBOSE, "  ..param1: %p", pParam1);
                LOGEX(MediaSourceMMSys, LOG_VERBOSE, "  ..param1: %u", pParam2);
                LOGEX(MediaSourceMMSys, LOG_VERBOSE, "  ..hdr buffer: %p", tBufferDesc->lpData);
                LOGEX(MediaSourceMMSys, LOG_VERBOSE, "  ..hdr buffer length: %u", tBufferDesc->dwBufferLength);
                LOGEX(MediaSourceMMSys, LOG_VERBOSE, "  ..hdr buffer recorded: %u", tBufferDesc->dwBytesRecorded);
                LOGEX(MediaSourceMMSys, LOG_VERBOSE, "  ..hdr user data: %u", tBufferDesc->dwUser);
            #endif

            // save current index in local variable
            int tCurIndex = tBufferDesc->dwUser;
            #ifdef MMSYS_DEBUG_PACKETS
                LOGEX(MediaSourceMMSys, LOG_VERBOSE, "Full buffer %d..", tCurIndex);
            #endif

            // set write pointer to next valid value for the buffer queue, set count of waiting buffers
            tSource->mMutexStateData.lock();

            // only process new audio data if all variables are already initiated
            if (tSource->mMediaSourceOpened)
            {
                if (tSource->mQueueSize < MEDIA_SOURCE_MMSYS_BUFFER_QUEUE_SIZE)
                {
                    // rescue captured data
                    tSource->mQueue[tSource->mQueueWritePtr].Size = (int)tBufferDesc->dwBytesRecorded;
                    memcpy(tSource->mQueue[tSource->mQueueWritePtr].Data, tBufferDesc->lpData, (size_t)tBufferDesc->dwBytesRecorded);
                    tSource->mQueueSize++;

                    // witch to next element in queue
                    tSource->mQueueWritePtr++;
                    if (tSource->mQueueWritePtr >= MEDIA_SOURCE_MMSYS_BUFFER_AMOUNT)
                        tSource->mQueueWritePtr = tSource->mQueueWritePtr - MEDIA_SOURCE_MMSYS_BUFFER_AMOUNT;

                    // re-use capture buffer
                    if (!tSource->mGrabbingStopped)
                    {
                        // insert new buffer in device queue
                        #ifdef MMSYS_DEBUG_PACKETS
                            LOGEX(MediaSourceMMSys, LOG_VERBOSE, "Adding capture buffer %d..", tCurIndex);
                        #endif
                        //tSource->mCaptureBufferDesc[tCurIndex].dwBytesRecorded = 0;
                        tResult = waveInAddBuffer(tSource->mCaptureHandle, tBufferDesc, sizeof(WAVEHDR));
                        if(tResult != MMSYSERR_NOERROR)
                        {
                            waveInGetErrorText(tResult, tErrorBuffer, 256);
                            LOGEX(MediaSourceMMSys, LOG_ERROR, "Can not register capture buffer %d for WaveIn device because of \"%s\"", tCurIndex, tErrorBuffer);
                            break;
                        }
                    }

                    // are we informed about new capture data?
                    if (pMessage == WIM_DATA)
                        tSource->mWaitCondition.SignalOne();
                }else
                    LOGEX(MediaSourceMMSys, LOG_ERROR, "Buffer queue full - dropping current data chunk");
            }

            tSource->mMutexStateData.unlock();

            break;
    }
}

int MediaSourceMMSys::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    // lock grabbing
    mGrabMutex.lock();

    if (mGrabbingStopped)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("audio source is paused");

        return -1;
    }

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        // acknowledge failed
        MarkGrabChunkFailed("audio source is closed");

        return -1;
    }

    mMutexStateData.lock();

    if (mQueueSize == 0)
    {
        #ifdef MMSYS_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Waiting for some new audio capture data");
        #endif

        // wait for capture event from static function
        if (!mWaitCondition.Wait(&mMutexStateData))
            LOG(LOG_ERROR, "Error when waiting for capture event");
    }
    #ifdef MMSYS_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Pending buffers %d", mQueueSize);
        LOG(LOG_VERBOSE, "Reading from buffer %d", mQueueReadPtr);
    #endif

    // get captured data from queue
    pChunkSize = (int)mQueue[mQueueReadPtr].Size;
    memcpy(pChunkBuffer, mQueue[mQueueReadPtr].Data, (size_t)pChunkSize);
    mQueueSize--;

    mQueueReadPtr++;
    if (mQueueReadPtr >= MEDIA_SOURCE_MMSYS_BUFFER_AMOUNT)
        mQueueReadPtr = mQueueReadPtr - MEDIA_SOURCE_MMSYS_BUFFER_AMOUNT;

    mMutexStateData.unlock();

    if ((pChunkSize < mSampleBufferSize) && (!mGrabbingStopped))
        LOG(LOG_INFO, "Captured data chunk is too short (%d < %d)", pChunkSize, mSampleBufferSize);

    #ifdef MMSYS_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Delivering packet with size %d", pChunkSize);
    #endif

    // re-encode the frame and write it to file
    if (mRecording)
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

bool MediaSourceMMSys::SupportsRecording()
{
    return true;
}

void MediaSourceMMSys::StopGrabbing()
{
    MMRESULT  tResult;
    char tErrorBuffer[256];

    MediaSource::StopGrabbing();

    if (mMediaSourceOpened)
    {
        tResult = waveInReset(mCaptureHandle);
        if(tResult != MMSYSERR_NOERROR)
        {
            waveInGetErrorText(tResult, tErrorBuffer, 256);
            LOG(LOG_ERROR, "Can not reset WaveIn device because of \"%s\"", tErrorBuffer);
        }
    }
}

string MediaSourceMMSys::GetSourceCodecStr()
{
    return "Raw";
}

string MediaSourceMMSys::GetSourceCodecDescription()
{
    return "Raw";
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
