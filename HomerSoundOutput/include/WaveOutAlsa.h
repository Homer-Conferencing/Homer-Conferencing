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
 * Purpose: wave out based on ALSA
 * Author:  Thomas Volkert
 * Since:   2010-12-11
 */

#ifdef LINUX
#ifndef _SOUNDOUT_WAVE_OUT_ALSA_
#define _SOUNDOUT_WAVE_OUT_ALSA_

#include <WaveOut.h>
#include <Header_Alsa.h>

namespace Homer { namespace SoundOutput {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define WOA_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class WaveOutAlsa:
    public WaveOut
{
public:
    WaveOutAlsa(std::string pDesiredDevice = "");

    /// The destructor
    virtual ~WaveOutAlsa();

    // playback control
    virtual void StopPlayback();

public:
    /* open/close */
    virtual bool OpenWaveOutDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseWaveOutDevice();
    /* device interface */
    virtual void getAudioDevices(AudioOutDevicesList &pAList);
    /* playback control */
    virtual bool WriteChunk(void* pChunkBuffer, int pChunkSize = 4096);

private:
    snd_pcm_t           *mPlaybackHandle;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
#endif
