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
 * Purpose: Wave output based on Alsa
 * Since:   2010-12-11
 */

//HINT: documentation: http://www.alsa-project.org/alsa-doc/alsa-lib/group___p_c_m.html

#include <WaveOutMMSys.h>
#include <Logger.h>

namespace Homer { namespace SoundOutput {

using namespace std;
using namespace Homer::Monitor;

WaveOutMMSys::WaveOutMMSys(string pDesiredDevice):
    WaveOut("MMSys: local playback")
{
    SelectDevice(pDesiredDevice);
    //mSampleBufferSize = MEDIA_SOURCE_ALSA_BUFFER_SIZE * 2 /* SND_PCM_FORMAT_S16_LE */ * 2 /* stereo */;
    mPlaybackHandle = NULL;
    mPlaybackEvent = NULL;
    LOG(LOG_VERBOSE, "Created");
}

WaveOutMMSys::~WaveOutMMSys()
{
    if (mWaveOutOpened)
        CloseWaveOutDevice();

    LOG(LOG_VERBOSE, "Destroyed");
}

void WaveOutMMSys::getAudioDevices(AudioOutDevicesList &pAList)
{
    AudioOutDeviceDescriptor tDevice;
    WAVEOUTCAPS tCaps;
    UINT tCapsSize = sizeof(tCaps);

    LOG(LOG_VERBOSE, "Enumerating hardware..");

    tDevice.Name = "auto";
    tDevice.Card = "";
    tDevice.Desc = "automatic device selection";
    tDevice.IoType = "Input/Output";
    pAList.push_front(tDevice);

    MMRESULT tRes;
    for (UINT i = 0; i < 20; i++)
    {
        if (waveOutGetDevCaps(i, (LPWAVEOUTCAPS)&tCaps, tCapsSize) == MMSYSERR_NOERROR)
        {
            LOG(LOG_INFO, "Found active MMSystem device %d", i);
            LOG(LOG_INFO, "  ..manufacturer identifier: %d", (int)tCaps.wMid);
            LOG(LOG_INFO, "  ..product identifier: %d", (int)tCaps.wPid);
            LOG(LOG_INFO, "  ..driver version: 0x%04X", (int)tCaps.vDriverVersion);
            LOG(LOG_INFO, "  ..product name: %s", (int)tCaps.szPname);
            LOG(LOG_INFO, "  ..channels: %d", (int)tCaps.wChannels);

            #ifdef MMSYS_OUT_DEBUG_PACKETS
                if (tCaps.dwSupport & WAVECAPS_LRVOLUME)
                    LOG(LOG_INFO, "  ..supporting separate left and right volume control");
                if (tCaps.dwSupport & WAVECAPS_PITCH)
                    LOG(LOG_INFO, "  ..supporting pitch control");
                if (tCaps.dwSupport & WAVECAPS_PLAYBACKRATE)
                    LOG(LOG_INFO, "  ..supporting playback rate control");
                if (tCaps.dwSupport & WAVECAPS_VOLUME)
                    LOG(LOG_INFO, "  ..supporting volume control");
                if (tCaps.dwSupport & WAVECAPS_SAMPLEACCURATE)
                    LOG(LOG_INFO, "  ..supporting sample-accurate position information");

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
            #endif

            tDevice.Name = "MMSYS: " + string(tCaps.szPname);
            tDevice.Card = (char)i + 48;
            tDevice.Desc = "Windows multimedia system (MMSYS) audio device " + tDevice.Card + " \"" + string(tCaps.szPname) + "\"";
            if (tDevice.Name.size())
                LOG(LOG_VERBOSE, "Found audio playback device: %s (card: %s)", tDevice.Name.c_str(), tDevice.Card.c_str());
            pAList.push_back(tDevice);
        }else
            break;
    }
}

bool WaveOutMMSys::OpenWaveOutDevice(int pSampleRate, bool pStereo)
{
    unsigned int tChannels = pStereo?2:1;
    int tErr;
    MMRESULT tResult;
    WAVEFORMATEX tFormat;
    char tErrorBuffer[256];

    LOG(LOG_VERBOSE, "Trying to open the audio source");

    if (mWaveOutOpened)
        return false;

    mSampleRate = pSampleRate;
    mStereo = pStereo;

    if ((mDesiredDevice == "") || (mDesiredDevice == "auto") || (mDesiredDevice == "automatic"))
        mDesiredDevice = toString((int)WAVE_MAPPER);

    mPlaybackEvent = CreateEvent(NULL, 1, 0, NULL);
    if(!mPlaybackEvent)
    {
        LOG(LOG_ERROR, "Can not create capture event object");
        return false;
    }

     // Specify recording parameters
    tFormat.wFormatTag = WAVE_FORMAT_PCM;   // simple, uncompressed format
    tFormat.nChannels = tChannels;          //  1 = mono, 2 = stereo
    tFormat.nSamplesPerSec = pSampleRate;   // default: 44100
    tFormat.nAvgBytesPerSec = pSampleRate * 2 * 2; // SamplesPerSec * Channels * BitsPerSample / 8
    tFormat.nBlockAlign = 2 * 2;            // Channels * BitsPerSample / 8
    tFormat.wBitsPerSample = 16;            //  16 for high quality, 8 for telephone quality
    tFormat.cbSize = 0;                     // no additional information appended

    LOG(LOG_VERBOSE, "Check if format is supported..");
    tResult = waveOutOpen(&mPlaybackHandle, (UINT)atoi(mDesiredDevice.c_str()), &tFormat, 0L, 0L, WAVE_FORMAT_QUERY);
    if(tResult == WAVERR_BADFORMAT)
    {
        waveInGetErrorText(tResult, tErrorBuffer, 256);
        LOG(LOG_ERROR, "Selected wave format isn't support because of \"%s\"", tErrorBuffer);
        return false;
    }

    LOG(LOG_VERBOSE, "Open WaveIn device..");
    tResult = waveOutOpen(&mPlaybackHandle, (UINT)atoi(mDesiredDevice.c_str()), &tFormat, (DWORD)&EventHandler, (DWORD)this, CALLBACK_FUNCTION | WAVE_FORMAT_DIRECT);
    if(tResult != MMSYSERR_NOERROR)
    {
        waveOutGetErrorText(tResult, tErrorBuffer, 256);
        LOG(LOG_ERROR, "Can not open WaveIn device because of \"%s\"", tErrorBuffer);
        return false;
    }

    mCurrentDevice = mDesiredDevice;

    LOG(LOG_INFO, "Opened...");
    LOG(LOG_INFO,"    ..sample rate: %d", pSampleRate);
    LOG(LOG_INFO,"    ..channels: %d", tChannels);
    LOG(LOG_INFO,"    ..desired device: %s", mDesiredDevice.c_str());
    LOG(LOG_INFO,"    ..selected device: %s", mCurrentDevice.c_str());
    LOG(LOG_INFO,"    ..sample format: 16 bit little endian");
    //LOG(LOG_INFO,"    ..sample buffer size: %d", mSampleBufferSize);

    mChunkNumber = 0;
    mWaveOutOpened = true;
    return true;
}

bool WaveOutMMSys::CloseWaveOutDevice()
{
    MMRESULT tResult = 0;
    char tErrorBuffer[256];

    LOG(LOG_VERBOSE, "Going to close");

    if (mWaveOutOpened)
    {
        mWaveOutOpened = false;

        LOG(LOG_VERBOSE, "Going to release capture event object");
        if (mPlaybackEvent != NULL)
            CloseHandle(mPlaybackEvent);

        LOG(LOG_VERBOSE, "Going to close WaveIn device");
        tResult = waveOutClose(mPlaybackHandle);
        if(tResult != MMSYSERR_NOERROR)
        {
            waveOutGetErrorText(tResult, tErrorBuffer, 256);
            LOG(LOG_ERROR, "Can not close WaveIn device because of \"%s\"", tErrorBuffer);
            return false;
        }

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mPlaybackStopped = false;

    return tResult;
}

void CALLBACK WaveOutMMSys::EventHandler(HWAVEOUT pPlaybackDevice, UINT pMessage, DWORD pInstance, DWORD pParam1, DWORD pParam2)
{
	WaveOutMMSys *tSink = (WaveOutMMSys*)pInstance;

    #ifdef MMSYS_OUT_DEBUG_PACKETS
        switch(pMessage)
        {
            case WOM_CLOSE:
                LOG(LOG_VERBOSE, "################## WaveOut event of type \"device closed\" occurred ######################");
                break;
            case WOM_OPEN:
                LOG(LOG_VERBOSE, "################## WaveOut event of type \"device opened\" occurred ######################");
                break;
            case WOM_DONE:
                LOG(LOG_VERBOSE, "################## WaveOut event of type \"playback done\" occurred ######################");
                break;
        }
    #endif

    // are we informed about new capture data?
//    if (pMessage == WIM_DATA)
//    {
//        SetEvent(tSource->mPlaybackEvent);
//    }
}

bool WaveOutMMSys::WriteChunk(void* pChunkBuffer, int pChunkSize)
{
	return false;
}

void WaveOutMMSys::Stop()
{
    MMRESULT  tResult;
    char tErrorBuffer[256];

    WaveOut::Stop();

    tResult = waveOutReset(mPlaybackHandle);
    if(tResult != MMSYSERR_NOERROR)
    {
        waveOutGetErrorText(tResult, tErrorBuffer, 256);
        LOG(LOG_ERROR, "Can not reset WaveOut device because of \"%s\"", tErrorBuffer);
    }
}

}} //namespaces
