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
 * Purpose: wave out based on Windows multimedia system
 * Since:   2010-12-11
 */

#if defined(WINDOWS)
#ifndef _SOUNDOUT_WAVE_OUT_MMSYS_
#define _SOUNDOUT_WAVE_OUT_MMSYS_

#include <WaveOut.h>
#include <Header_MMSys.h>

namespace Homer { namespace SoundOutput {

///////////////////////////////////////////////////////////////////////////////

//#define MMSYS_OUT_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

#ifndef CALLBACK
#define CALLBACK __stdcall
#endif

///////////////////////////////////////////////////////////////////////////////

class WaveOutMMSys:
    public WaveOut
{
public:
    WaveOutMMSys(std::string pDesiredDevice = "");

    /// The destructor
    virtual ~WaveOutMMSys();

    // playback control
    virtual void Stop();

public:
    /* open/close */
    virtual bool OpenWaveOutDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseWaveOutDevice();
    /* device interface */
    virtual void getAudioDevices(AudioOutDevicesList &pAList);
    /* playback control */
    virtual bool WriteChunk(void* pChunkBuffer, int pChunkSize = 4096);

private:
    static CALLBACK void EventHandler(HWAVEOUT pPlaybackDevice, UINT pMessage, DWORD pInstance, DWORD pParam1, DWORD pParam2);

    HANDLE          mPlaybackEvent;
    HWAVEOUT        mPlaybackHandle;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
#endif
