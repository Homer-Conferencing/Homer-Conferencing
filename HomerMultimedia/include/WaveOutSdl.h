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
 * Purpose: wave out based on SDL
 * Since:   2012-05-13
 */

#ifndef _MULTIMEDIA_WAVE_OUT_SDL_
#define _MULTIMEDIA_WAVE_OUT_SDL_

#include <WaveOut.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define WOS_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class WaveOutSdl:
    public WaveOut
{
public:
	WaveOutSdl(std::string pOutputName, std::string pDesiredDevice = "");

    /// The destructor
    virtual ~WaveOutSdl();

    // playback control
    virtual bool Play();
    virtual void Stop();

    /* volume control */
    virtual void SetVolume(int pValue);

public:
    /* open/close */
    virtual bool OpenWaveOutDevice(int pSampleRate = 44100, int pOutputChannels = 2);
    virtual bool CloseWaveOutDevice();
    /* device interface */
    virtual void getAudioDevices(AudioDevices &pAList);

private:
    virtual void AssignThreadName();
    virtual void DoWriteChunk(char *ChunkBuffer, int pChunkSize);

    static void SdlAudioInit(std::string pDeviceName);

    /* SDL init. */
    static Mutex        mSdlInitMutex;
    static bool         mSdlInitiated;
    /* playback */
    int            		mAudioChannel;
    /* recursion logger */
    static int			mOpenStreams;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
