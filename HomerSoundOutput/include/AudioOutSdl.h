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
 * Purpose: header file for universal SDL based audio output
 * Author:  Stefan Koegel, Thomas Volkert
 * Since:   2009-03-18
 */

#ifndef _SOUNDOUT_AUDIO_OUT_SDL_
#define _SOUNDOUT_AUDIO_OUT_SDL_

#include <HBMutex.h>

#include <list>
#include <map>
#include <string>

///SDL 1.3
//#include </usr/local/include/SDL/SDL_audio.h>

using namespace Homer::Base;

namespace Homer { namespace SoundOutput {

///////////////////////////////////////////////////////////////////////////////

//#define DEBUG_AUDIO_OUT_SDL

#define AUDIO_BUFFER_QUEUE_LIMIT                2

#define AUDIOOUTSDL AudioOutSdl::getInstance()
// amount of mixing channels (independent from stereo/mono)
#define DEFAULT_CHANNEL_COUNT                   16

///////////////////////////////////////////////////////////////////////////////
struct AudioOutInfo
{
    std::list<std::string> Drivers;
    std::string CurDriver;
    std::list<std::string> Devices;
};

/**
 * @brief A class for audio output of raw PCM samples
 */
class AudioOutSdl
{
public:

    AudioOutSdl();

    /// The destructor
    virtual ~AudioOutSdl();

    static AudioOutSdl sAudioOut;
    static AudioOutSdl& getInstance();

public:
    /**
     * @brief open playback device
     * @param[in] pSampleRate - Sampling Frequency, typically 44100 (44khz)
     * @param[in] pStereo - true Stereo, false Mono
     * @param[in] pDriver - audio output driver
     * @param[in] pDevice - audio output device
     * @return true if successful else false
     *
     * possible Drivers:
     * OpenBSD, dsp, alsa, audio, AL, artsc, esd, nas, dma, dsound, waveout, baudio, sndmgr, paud, AHI, disk
     *
     * possible Devices: (SDL 1.3 feature, please use "auto" if SDL Version <= 1.2)
     *
     * "QueryAudioOutDevices" delivers appropriate values
     *
     */
    bool OpenPlaybackDevice(int pSampleRate = 44100, bool pStereo = true, std::string pDriver = "alsa", std::string pDevice = "auto");
    void ClosePlaybackDevice();
    int AllocateChannel();
    void ReleaseChannel(int pChannel);
    bool Play(int pChannel);
    bool Stop(int pChannel);

    /*
     * Set the volume in the range of 0-128 of a specific channel
     * If the specified channel is -1, set volume for all channels.
     * Returns the original volume.
     * If the specified volume is -1, just return the current volume.
     */
    int SetVolume(int pChannel, int pVolume);

    /**
     * @brief add audio buffer to channels output queue
     * @param pChannel - channel id
     * @param pBuffer - pointer to audio buffer
     * @param pBufferSize - size of audio buffer
     * @param pLimitBucket - limits the amount of elements within the chunk queue per channel, exact limit is hard coded
     * @return false if audio output is closed or memory allocation failed
     */
    bool Enqueue(int pChannel, void *pBuffer, int pBufferSize = 4096, bool pLimitBucket = true);

    /**
     * @brief add file to channels output queue
     * @param pChannel - channel id
     * @param File - location of audio file
     * @param pLimitBucket - limits the amount of elements within the chunk queue per channel, exat limit is hard coded
     * @return false if decoding of audio samples failed, audio output is closed or memory allocation failed
     */
    bool Enqueue(int pChannel, std::string File, bool pLimitBucket = true);

    /**
     * @brief Query Audio Device Information
     * @return deviceInfo List
     */
    AudioOutInfo QueryAudioOutDevices();


    void ClearChunkList(int pChannel);

private:
    static void PlayerCallBack(int channel);

    void ClearChunkListInternal(int pChannel);

    bool mAudioOutOpened;
    int mChannels;

    struct ChannelEntry
    {
        std::list<void*> Chunks;
        void*   LastChunk;
        Mutex   mMutex;
        bool    Assigned;
        bool    IsPlaying;
    };
    std::map<int, ChannelEntry*> mChannelMap;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
